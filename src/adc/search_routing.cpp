#include "dc24h/adc/search_routing.hpp"
#include "dc24h/adc/serializer.hpp"

namespace dc24h::adc {
namespace {
SearchRoutingError convert(search::Error error) noexcept {
    switch (error) {
    case search::Error::missing_sender: case search::Error::sender_not_ready: return SearchRoutingError::invalid_state;
    case search::Error::wrong_source: return SearchRoutingError::wrong_source;
    case search::Error::recipient_unavailable: return SearchRoutingError::recipient_unavailable;
    case search::Error::foreign_plan: return SearchRoutingError::foreign_plan;
    case search::Error::missing_sink: return SearchRoutingError::missing_sink;
    default: return SearchRoutingError::invalid_message;
    }
}
SearchRoutingError convert(search::TokenError error) noexcept {
    if (error == search::TokenError::duplicate_token) { return SearchRoutingError::duplicate_token; }
    if (error == search::TokenError::total_limit || error == search::TokenError::owner_limit || error == search::TokenError::id_exhausted ||
        error == search::TokenError::time_exhausted) { return SearchRoutingError::search_limit; }
    if (error == search::TokenError::inactive_search) { return SearchRoutingError::inactive_search; }
    return SearchRoutingError::invalid_message;
}
} // namespace
std::variant<RoutedSearch,SearchRoutingError> SearchRouter::prepare(users::SessionId sender, const Message& message, users::TimePoint now) {
    if (const auto error = core_.validate_sender(sender,message.source ? std::string_view(*message.source) : std::string_view{})) { return convert(*error); }
    auto checked = validate_sch(message);
    if (const auto* error = std::get_if<SchError>(&checked)) {
        return *error == SchError::no_criteria ? SearchRoutingError::no_criteria : SearchRoutingError::invalid_message;
    }
    const auto& request = std::get<SchRequest>(checked);
    search::Audience audience;
    if (message.target) {
        audience.scope = message.type == MessageType::echo ? search::Scope::echo : search::Scope::direct;
        audience.target = core_.resolve_target(*message.target);
        if (!audience.target) { return SearchRoutingError::recipient_unavailable; }
    }
    // Finish frame/attempt allocations before publishing any active token.
    auto encoded = serialize(request.message());
    auto* wire = std::get_if<std::string>(&encoded);
    if (!wire) { return SearchRoutingError::invalid_message; }
    auto frame = SharedSearchFrame(new SearchFrame(request.message(),std::move(*wire)));
    auto attempt = std::make_shared<RoutedSearch::Attempt>();
    if (request.criteria().token) {
        auto prepared = core_.start(sender,*message.source,request.criteria(),now,request.selection(),audience);
        if (const auto* error = std::get_if<search::Error>(&prepared)) { return convert(*error); }
        if (const auto* error = std::get_if<search::TokenError>(&prepared)) { return convert(*error); }
        if (std::holds_alternative<search::QueryError>(prepared)) { return SearchRoutingError::invalid_message; }
        return RoutedSearch(std::get<search::ActivePlan>(std::move(prepared)),std::move(frame),std::move(attempt));
    }
    auto prepared = core_.prepare(sender,*message.source,request.criteria(),request.selection(),audience);
    if (const auto* error = std::get_if<search::Error>(&prepared)) { return convert(*error); }
    if (std::holds_alternative<search::QueryError>(prepared)) { return SearchRoutingError::invalid_message; }
    return RoutedSearch(std::get<search::Plan>(std::move(prepared)),std::move(frame),std::move(attempt));
}
bool SearchRouter::cancel(const RoutedSearch& message) noexcept {
    const auto* active = std::get_if<search::ActivePlan>(&message.admission_);
    return active && core_.complete(*active);
}
SearchDispatch SearchRouter::dispatch(const RoutedSearch& message, const Sink& sink, users::TimePoint now) {
    if (!message.frame_ || !message.attempt_) { return SearchRoutingError::invalid_message; }
    if (!core_.owns(message.plan())) { return SearchRoutingError::foreign_plan; }
    if (!sink) { return SearchRoutingError::missing_sink; }
    if (message.attempt_->started) { return SearchRoutingError::already_dispatched; }
    message.attempt_->started = true;
    const auto forward = [&](users::SessionId recipient,const search::Plan&) { return sink(recipient,message.frame_); };
    // Reference-wrapper construction guarantees no std::function allocation.
    const search::Core::Sink send(std::cref(forward));
    auto result = std::visit([&](const auto& admission) -> SearchDispatch {
        using T = std::decay_t<decltype(admission)>;
        if constexpr (std::is_same_v<T,search::ActivePlan>) {
            const auto report = core_.dispatch(admission,send,now);
            if (const auto* error = std::get_if<search::Error>(&report)) { return convert(*error); }
            if (const auto* error = std::get_if<search::TokenError>(&report)) { return convert(*error); }
            return std::get<search::DispatchReport>(report);
        } else {
            const auto report = core_.dispatch(admission,send);
            if (const auto* error = std::get_if<search::Error>(&report)) { return convert(*error); }
            return std::get<search::DispatchReport>(report);
        }
    },message.admission_);
    const auto* report = std::get_if<search::DispatchReport>(&result);
    if (!report || report->queued == 0) { (void)cancel(message); }
    return result;
}
} // namespace dc24h::adc
