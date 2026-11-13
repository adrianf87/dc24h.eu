#include "dc24h/adc/rcm_routing.hpp"
#include "dc24h/adc/serializer.hpp"

namespace dc24h::adc {
namespace {
RcmRoutingError convert(transfers::Error error) noexcept {
    using enum transfers::Error;
    switch (error) {
    case missing_sender: case sender_not_ready: return RcmRoutingError::invalid_state;
    case wrong_source: return RcmRoutingError::wrong_source;
    case recipient_unavailable: return RcmRoutingError::recipient_unavailable;
    case incompatible_connectivity: return RcmRoutingError::incompatible_connectivity;
    case foreign_plan: return RcmRoutingError::foreign_plan;
    case missing_sink: return RcmRoutingError::missing_sink;
    case already_consumed: return RcmRoutingError::already_dispatched;
    default: return RcmRoutingError::invalid_message;
    }
}
} // namespace
std::variant<RoutedRcm, RcmRoutingError> RcmRouter::prepare(users::SessionId sender, const Message& message) {
    if (const auto error = core_.validate_sender(sender, message.source ? std::string_view(*message.source) : std::string_view{})) { return convert(*error); }
    auto checked = validate_rcm(message);
    auto* request = std::get_if<RcmRequest>(&checked);
    if (!request) { return RcmRoutingError::invalid_message; }
    const auto target = core_.resolve_target(*message.target);
    if (!target) { return RcmRoutingError::recipient_unavailable; }
    if (*target == sender) { return RcmRoutingError::invalid_message; }
    auto family = transfers::AddressFamily::ipv4;
    const auto eligible = [&](auto f) { return !core_.tcp_address(sender, f) && core_.tcp_address(*target, f).has_value(); };
    if (!eligible(family)) {
        family = transfers::AddressFamily::ipv6;
        if (!eligible(family)) { return RcmRoutingError::incompatible_connectivity; }
    }
    transfers::RequestData data{transfers::Kind::reverse_connect, family, request->protocol(), std::nullopt, request->token()};
    auto admission = core_.prepare(sender, *message.source, *message.target, data);
    if (const auto* error = std::get_if<transfers::Error>(&admission)) { return convert(*error); }
    auto* plan = std::get_if<transfers::Plan>(&admission);
    if (!plan) { return RcmRoutingError::invalid_message; }
    auto encoded = serialize(request->message());
    auto* wire = std::get_if<std::string>(&encoded);
    if (!wire) { return RcmRoutingError::invalid_message; }
    auto frame = SharedRcmFrame(new RcmFrame(request->message(), std::move(*wire)));
    return RoutedRcm(domain_, std::move(*plan), std::move(frame));
}
std::variant<RcmDispatchReport, RcmRoutingError> RcmRouter::dispatch(const RoutedRcm& routed, const Sink& sink) {
    if (routed.domain_ != domain_) { return RcmRoutingError::foreign_plan; }
    if (!routed.frame_) { return RcmRoutingError::invalid_message; }
    if (!sink) { return RcmRoutingError::missing_sink; }
    const auto frame = routed.frame_;
    const auto sender = routed.plan_.sender();
    const auto family = routed.plan_.request().family();
    auto target_sink = [&](users::SessionId recipient, const transfers::Plan&) { return sink(recipient, frame); };
    const auto result = core_.dispatch(routed.plan_, std::ref(target_sink));
    if (const auto* error = std::get_if<transfers::Error>(&result)) { return convert(*error); }
    RcmDispatchReport report;
    if (std::get<transfers::State>(result) == transfers::State::queued) { ++report.queued; } else { ++report.refused; }
    if (frame->message().type == MessageType::echo) {
        if (core_.validate_sender(sender, *frame->message().source) || core_.tcp_address(sender, family)) { ++report.canceled; }
        else if (sink(sender, frame)) { ++report.queued; } else { ++report.refused; }
    }
    return report;
}
bool RcmRouter::cancel(const RoutedRcm& message) noexcept { return message.domain_ == domain_ && core_.cancel(message.plan_); }
} // namespace dc24h::adc
