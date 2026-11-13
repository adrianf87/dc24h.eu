#include "dc24h/transfers/core.hpp"

namespace dc24h::transfers {
Plan::Plan(std::shared_ptr<const detail::Domain> domain, users::SessionId sender, users::SessionId target,
           std::string_view source, std::string_view target_key, Request request, users::AddressBytes address)
    : domain_(std::move(domain)), sender_(sender), target_(target), source_(source), target_key_(target_key),
      request_(std::move(request)), address_(address), attempt_(std::make_shared<detail::Attempt>()) {}
std::optional<Error> Core::validate_sender(users::SessionId sender, std::string_view source) const noexcept {
    const auto* model = sessions_.find(sender);
    if (!model) { return Error::missing_sender; }
    if (!model->ready() || !model->user()) { return Error::sender_not_ready; }
    if (model->identity()->sid() != source) { return Error::wrong_source; }
    return {};
}
std::optional<users::AddressBytes> Core::tcp_address(users::SessionId owner, AddressFamily family) const noexcept {
    const auto* model = sessions_.find(owner);
    if (!model || !model->ready() || !model->user() || !users::tcp_active(model->user()->profile(), family)) { return {}; }
    return users::public_address(model->user()->profile(), family);
}
std::optional<users::SessionId> Core::resolve_target(std::string_view target) const noexcept {
    const auto* model = sessions_.by_sid(target);
    return model && model->ready() && model->user() ? std::optional{model->id()} : std::nullopt;
}
std::optional<users::AddressBytes> Core::address(const users::Session& sender, const users::Session& target, const Request& request) const noexcept {
    const auto& origin = sender.user()->profile();
    const auto& recipient = target.user()->profile();
    if (request.kind() == Kind::connect_to_me) {
        if (!users::tcp_active(origin, request.family())) { return {}; }
        return users::public_address(origin, request.family());
    }
    if (users::tcp_active(origin, request.family()) || !users::tcp_active(recipient, request.family())) { return {}; }
    return users::public_address(recipient, request.family());
}
Preparation Core::prepare(users::SessionId sender, std::string_view source, std::string_view target, const RequestData& data) const {
    if (const auto error = validate_sender(sender, source)) { return *error; }
    const auto* destination = sessions_.by_sid(target);
    if (!destination || !destination->ready() || !destination->user()) { return Error::recipient_unavailable; }
    if (destination->id() == sender) { return Error::self_connection; }
    auto validated = Request::create(data);
    if (const auto* error = std::get_if<RequestError>(&validated)) { return *error; }
    auto request = std::get<Request>(std::move(validated));
    const auto endpoint = address(*sessions_.find(sender), *destination, request);
    if (!endpoint) { return Error::incompatible_connectivity; }
    return Plan(domain_, sender, destination->id(), source, target, std::move(request), *endpoint);
}
std::variant<State, Error> Core::dispatch(const Plan& plan, const Sink& sink) const {
    if (!owns(plan)) { return Error::foreign_plan; }
    if (!sink) { return Error::missing_sink; }
    if (plan.state() != State::prepared) { return Error::already_consumed; }
    const auto attempt = plan.attempt_;
    attempt->state = State::canceled;
    if (const auto error = validate_sender(plan.sender(), plan.source())) { return *error; }
    const auto* destination = sessions_.find(plan.target());
    if (!destination || !destination->ready() || !destination->user() || destination->identity()->sid() != plan.target_key()) {
        return Error::recipient_unavailable;
    }
    const auto endpoint = address(*sessions_.find(plan.sender()), *destination, plan.request());
    if (!endpoint || *endpoint != plan.listener_address()) { return Error::incompatible_connectivity; }
    attempt->state = State::dispatching;
    try {
        attempt->state = sink(plan.target(), plan) ? State::queued : State::refused;
        return attempt->state;
    } catch (...) {
        attempt->state = State::failed;
        throw;
    }
}
bool Core::cancel(const Plan& plan) const noexcept {
    if (!owns(plan) || plan.state() != State::prepared) { return false; }
    plan.attempt_->state = State::canceled;
    return true;
}
} // namespace dc24h::transfers
