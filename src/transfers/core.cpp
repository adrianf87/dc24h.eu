#include "dc24h/transfers/core.hpp"

namespace dc24h::transfers {
namespace {
Error permission_error(PermissionError error) noexcept { return error == PermissionError::disabled ? Error::disabled : Error::forbidden; }
}
std::optional<Permissions> Core::permissions(users::SessionId id) const noexcept {
    const auto* session = sessions_.find(id);
    if (!session || !session->ready() || !session->user()) { return {}; }
    return authority_ ? authority_->connection_permissions(id) : std::optional{settings_.guests};
}
std::optional<PermissionError> Core::authorize_sender(users::SessionId sender, Kind kind) const noexcept {
    const auto origin = permissions(sender);
    if (!origin) { return PermissionError::forbidden; }
    return authorize(settings_, *origin, {}, kind);
}
std::optional<PermissionError> Core::authorize_pair(users::SessionId sender, users::SessionId target, Kind kind) const noexcept {
    const auto origin = permissions(sender), recipient = permissions(target);
    if (!origin || !recipient) { return PermissionError::forbidden; }
    return authorize(settings_, *origin, *recipient, kind);
}
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
    const auto snapshot = capabilities(owner);
    return snapshot ? snapshot->listener(family) : std::nullopt;
}
std::optional<ClientCapabilities> Core::capabilities(users::SessionId owner) const noexcept {
    const auto* model = sessions_.find(owner);
    if (!model || !model->ready() || !model->user()) { return {}; }
    return client_capabilities(model->user()->profile());
}
std::optional<users::SessionId> Core::resolve_target(std::string_view target) const noexcept {
    const auto* model = sessions_.by_sid(target);
    return model && model->ready() && model->user() ? std::optional{model->id()} : std::nullopt;
}
std::optional<users::AddressBytes> Core::address(const users::Session& sender, const users::Session& target, const Request& request) const noexcept {
    return connection_address(client_capabilities(sender.user()->profile()), client_capabilities(target.user()->profile()),
        request.kind(), request.family());
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
    if (const auto error = authorize_pair(sender, destination->id(), request.kind())) { return permission_error(*error); }
    if (!settings_.allowed_protocols.allows(request.protocol())) { return Error::protocol_forbidden; }
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
    if (const auto error = authorize_pair(plan.sender(), plan.target(), plan.request().kind())) { return permission_error(*error); }
    if (!settings_.allowed_protocols.allows(plan.request().protocol())) { return Error::protocol_forbidden; }
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
