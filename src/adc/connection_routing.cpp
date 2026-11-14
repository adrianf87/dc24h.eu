#include "dc24h/adc/connection_routing.hpp"
#include "dc24h/adc/ctm.hpp"
#include "dc24h/adc/rcm.hpp"
#include "dc24h/adc/serializer.hpp"

namespace dc24h::adc {
namespace {
ConnectionRoutingError convert(transfers::Error error, transfers::Kind kind) noexcept {
    using enum transfers::Error;
    switch (error) {
    case missing_sender: case sender_not_ready: return ConnectionRoutingError::invalid_state;
    case wrong_source: return ConnectionRoutingError::wrong_source;
    case recipient_unavailable: return ConnectionRoutingError::recipient_unavailable;
    case incompatible_connectivity: return kind == transfers::Kind::connect_to_me ?
        ConnectionRoutingError::inactive_sender : ConnectionRoutingError::incompatible_connectivity;
    case foreign_plan: return ConnectionRoutingError::foreign_plan;
    case missing_sink: return ConnectionRoutingError::missing_sink;
    case already_consumed: return ConnectionRoutingError::already_dispatched;
    case disabled: return ConnectionRoutingError::disabled;
    case forbidden: return ConnectionRoutingError::forbidden;
    case protocol_forbidden: return ConnectionRoutingError::protocol_forbidden;
    default: return ConnectionRoutingError::invalid_message;
    }
}
} // namespace
ConnectionPreparation ConnectionRouter::prepare(users::SessionId sender, const Message& message) {
    if (const auto error = core_.validate_sender(sender, message.source ? std::string_view(*message.source) : std::string_view{})) {
        return convert(*error, transfers::Kind::connect_to_me);
    }
    if (accepted_ != Command::unknown && message.kind != accepted_) { return ConnectionRoutingError::invalid_message; }
    if (message.kind == Command::ctm) {
        auto checked = validate_ctm(message);
        const auto* request = std::get_if<CtmRequest>(&checked);
        if (!request) { return ConnectionRoutingError::invalid_message; }
        const auto source = core_.capabilities(sender);
        const auto path = transfers::select_connection(*source, {}, transfers::Kind::connect_to_me);
        if (!path) { return ConnectionRoutingError::inactive_sender; }
        const transfers::RequestData data{transfers::Kind::connect_to_me, path->family, request->protocol(), request->port(), request->token()};
        return admit(sender, request->message(), data);
    }
    if (message.kind == Command::rcm) {
        auto checked = validate_rcm(message);
        const auto* request = std::get_if<RcmRequest>(&checked);
        if (!request) { return ConnectionRoutingError::invalid_message; }
        const auto target = core_.resolve_target(*message.target);
        if (!target) { return ConnectionRoutingError::recipient_unavailable; }
        if (*target == sender) { return ConnectionRoutingError::invalid_message; }
        const auto source = core_.capabilities(sender), recipient = core_.capabilities(*target);
        const auto path = transfers::select_connection(*source, *recipient, transfers::Kind::reverse_connect);
        if (!path) { return ConnectionRoutingError::incompatible_connectivity; }
        const transfers::RequestData data{transfers::Kind::reverse_connect, path->family, request->protocol(), std::nullopt, request->token()};
        return admit(sender, request->message(), data);
    }
    return ConnectionRoutingError::invalid_message;
}
ConnectionPreparation ConnectionRouter::admit(users::SessionId sender, const Message& checked, const transfers::RequestData& data) {
    auto admission = core_.prepare(sender, *checked.source, *checked.target, data);
    if (const auto* error = std::get_if<transfers::Error>(&admission)) { return convert(*error, data.kind); }
    auto* plan = std::get_if<transfers::Plan>(&admission);
    if (!plan) { return ConnectionRoutingError::invalid_message; }
    auto encoded = serialize(checked);
    auto* wire = std::get_if<std::string>(&encoded);
    if (!wire) { return ConnectionRoutingError::invalid_message; }
    auto frame = SharedConnectionFrame(new ConnectionFrame(checked, std::move(*wire)));
    return RoutedConnection(domain_, std::move(*plan), std::move(frame));
}
ConnectionDispatch ConnectionRouter::dispatch(const RoutedConnection& routed, const Sink& sink) {
    if (routed.domain_ != domain_) { return ConnectionRoutingError::foreign_plan; }
    if (!routed.frame_) { return ConnectionRoutingError::invalid_message; }
    if (!sink) { return ConnectionRoutingError::missing_sink; }
    // Retain the immutable frame and fixed metadata before invoking user code.
    const auto frame = routed.frame_;
    const auto sender = routed.plan_.sender();
    const auto family = routed.plan_.request().family();
    const auto kind = routed.plan_.request().kind();
    const auto address = routed.plan_.listener_address();
    const auto echo = routed.count_ == 2;
    auto target_sink = [&](users::SessionId recipient, const transfers::Plan&) { return sink(recipient, frame); };
    const auto result = core_.dispatch(routed.plan_, std::ref(target_sink));
    if (const auto* error = std::get_if<transfers::Error>(&result)) { return convert(*error, kind); }
    ConnectionDispatchReport report;
    if (std::get<transfers::State>(result) == transfers::State::queued) { ++report.queued; } else { ++report.refused; }
    if (echo) {
        const auto path = core_.tcp_address(sender, family);
        const bool changed = kind == transfers::Kind::connect_to_me ? path != address : path.has_value();
        if (core_.validate_sender(sender, *frame->message().source) || changed || core_.authorize_sender(sender, kind)) { ++report.canceled; }
        else if (sink(sender, frame)) { ++report.queued; } else { ++report.refused; }
    }
    return report;
}
bool ConnectionRouter::cancel(const RoutedConnection& routed) noexcept { return routed.domain_ == domain_ && core_.cancel(routed.plan_); }
} // namespace dc24h::adc
