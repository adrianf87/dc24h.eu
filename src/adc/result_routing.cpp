#include "dc24h/adc/result_routing.hpp"
#include "dc24h/adc/serializer.hpp"

namespace dc24h::adc {
namespace {
ResultRoutingError convert(search::Error error) noexcept {
    return error == search::Error::wrong_source ? ResultRoutingError::wrong_source : ResultRoutingError::invalid_state;
}
} // namespace
std::variant<RoutedResult, ResultRoutingError> ResultRouter::prepare(
    users::SessionId sender, const Message& message, users::TimePoint now) {
    if (const auto error = core_.validate_sender(sender, message.source ? std::string_view(*message.source) : std::string_view{})) {
        return convert(*error);
    }
    auto checked = validate_res(message);
    auto* request = std::get_if<ResRequest>(&checked);
    if (!request) { return ResultRoutingError::invalid_message; }
    const auto recipient = core_.resolve_target(*message.target);
    if (!recipient) { return ResultRoutingError::recipient_unavailable; }
    std::optional<search::ActiveSearch> active;
    if (request->token()) {
        active = core_.find_search(*recipient, *request->token(), now);
        if (!active) { return ResultRoutingError::inactive_search; }
    }
    auto encoded = serialize(request->message());
    auto* wire = std::get_if<std::string>(&encoded);
    if (!wire) { return ResultRoutingError::invalid_message; }
    auto frame = SharedResultFrame(new ResultFrame(request->message(), std::move(*wire)));
    return RoutedResult(domain_, sender, *recipient, std::move(frame), request->token(), active,
                        std::make_shared<RoutedResult::Attempt>());
}
ResultDispatch ResultRouter::dispatch(const RoutedResult& result, const Sink& sink, users::TimePoint now) {
    if (result.domain_ != domain_) { return ResultRoutingError::foreign_plan; }
    if (!result.frame_ || !result.attempt_) { return ResultRoutingError::invalid_message; }
    if (!sink) { return ResultRoutingError::missing_sink; }
    if (result.attempt_->started) { return ResultRoutingError::already_dispatched; }
    result.attempt_->started = true;
    const auto& message = result.frame_->message();
    if (const auto error = core_.validate_sender(result.sender_, *message.source)) { return convert(*error); }
    if (core_.resolve_target(*message.target) != result.recipient_) { return ResultRoutingError::recipient_unavailable; }
    if (result.token_ && core_.find_search(result.recipient_, *result.token_, now) != result.search_) {
        return ResultRoutingError::inactive_search;
    }
    // Result receipt never renews/completes a search: other responses may follow.
    search::DispatchReport report;
    if (sink(result.recipient_, result.frame_)) { report.queued = 1; }
    else { report.refused = 1; }
    return report;
}
} // namespace dc24h::adc
