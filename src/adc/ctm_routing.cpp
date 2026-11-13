#include "dc24h/adc/ctm_routing.hpp"
#include "dc24h/adc/serializer.hpp"

namespace dc24h::adc {
namespace {
CtmRoutingError convert(transfers::Error error) noexcept {
    using enum transfers::Error;
    switch (error) {
    case missing_sender: case sender_not_ready: return CtmRoutingError::invalid_state;
    case wrong_source: return CtmRoutingError::wrong_source;
    case recipient_unavailable: return CtmRoutingError::recipient_unavailable;
    case incompatible_connectivity: return CtmRoutingError::inactive_sender;
    case foreign_plan: return CtmRoutingError::foreign_plan;
    case missing_sink: return CtmRoutingError::missing_sink;
    case already_consumed: return CtmRoutingError::already_dispatched;
    default: return CtmRoutingError::invalid_message;
    }
}
} // namespace
std::variant<RoutedCtm, CtmRoutingError> CtmRouter::prepare(users::SessionId sender, const Message& message) {
    if (const auto error = core_.validate_sender(sender, message.source ? std::string_view(*message.source) : std::string_view{})) { return convert(*error); }
    auto checked = validate_ctm(message);
    auto* request = std::get_if<CtmRequest>(&checked);
    if (!request) { return CtmRoutingError::invalid_message; }
    auto family = transfers::AddressFamily::ipv4;
    if (!core_.tcp_address(sender, family)) {
        family = transfers::AddressFamily::ipv6;
        if (!core_.tcp_address(sender, family)) { return CtmRoutingError::inactive_sender; }
    }
    transfers::RequestData data{transfers::Kind::connect_to_me, family, request->protocol(), request->port(), request->token()};
    auto admission = core_.prepare(sender, *message.source, *message.target, data);
    if (const auto* error = std::get_if<transfers::Error>(&admission)) { return convert(*error); }
    auto* plan = std::get_if<transfers::Plan>(&admission);
    if (!plan) { return CtmRoutingError::invalid_message; }
    auto encoded = serialize(request->message());
    auto* wire = std::get_if<std::string>(&encoded);
    if (!wire) { return CtmRoutingError::invalid_message; }
    auto frame = SharedCtmFrame(new CtmFrame(request->message(), std::move(*wire)));
    return RoutedCtm(domain_, std::move(*plan), std::move(frame));
}
std::variant<CtmDispatchReport, CtmRoutingError> CtmRouter::dispatch(const RoutedCtm& routed, const Sink& sink) {
    if (routed.domain_ != domain_) { return CtmRoutingError::foreign_plan; }
    if (!routed.frame_) { return CtmRoutingError::invalid_message; }
    if (!sink) { return CtmRoutingError::missing_sink; }
    // Retain owned state across callbacks that may release caller-held copies.
    const auto frame = routed.frame_;
    const auto sender = routed.plan_.sender();
    const auto family = routed.plan_.request().family();
    const auto address = routed.plan_.listener_address();
    auto target_sink = [&](users::SessionId recipient, const transfers::Plan&) { return sink(recipient, frame); };
    const auto result = core_.dispatch(routed.plan_, std::ref(target_sink));
    if (const auto* error = std::get_if<transfers::Error>(&result)) { return convert(*error); }
    CtmDispatchReport report;
    if (std::get<transfers::State>(result) == transfers::State::queued) { ++report.queued; } else { ++report.refused; }
    if (frame->message().type == MessageType::echo) {
        if (core_.validate_sender(sender, *frame->message().source) || core_.tcp_address(sender, family) != address) {
            ++report.canceled;
        } else if (sink(sender, frame)) { ++report.queued; } else { ++report.refused; }
    }
    return report;
}
bool CtmRouter::cancel(const RoutedCtm& message) noexcept { return message.domain_ == domain_ && core_.cancel(message.plan_); }
} // namespace dc24h::adc
