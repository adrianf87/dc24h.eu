#include "dc24h/adc/inf_updates.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/user.hpp"
#include "dc24h/adc/user_login.hpp"

#include <type_traits>
#include <utility>

namespace dc24h::adc {
std::string_view inf_update_status(const InfUpdateFailure& error) noexcept {
    return std::visit([](const auto& value) -> std::string_view {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T,ClientSessionError>) {
            return std::visit([](auto inner) -> std::string_view {
                using E = decltype(inner);
                if constexpr (std::is_same_v<E,InfError>) { return inf_status_code(inner); }
                else if constexpr (std::is_same_v<E,StateError>) { return inner == StateError::invalid_state ? "244" : "240"; }
                else { return "240"; }
            },value);
        } else if constexpr (std::is_same_v<T,users::RegistryError> || std::is_same_v<T,users::SessionError>) {
            return user_login_status(value);
        } else { return value == InfUpdateError::invalid_public_user ? "243" : "210"; }
    },error);
}
InfUpdateResult InfUpdates::apply(users::SessionId id, ClientSession& client,
        const Message& input, users::TimePoint now) {
    static_assert(std::is_nothrow_move_constructible_v<InfUpdateResult>);
    const auto* model = sessions_.find(id);
    if (!model) { return InfUpdateFailure{users::RegistryError::missing}; }
    if (!model->ready() || client.state() != ConnectionState::normal) {
        return InfUpdateFailure{users::SessionError::invalid_state};
    }
    const auto sid = Sid::parse(client.sid());
    if (!sid || !client.info()) { return InfUpdateFailure{InfUpdateError::identity_mismatch}; }
    const auto previous_user = user_from_info(*client.info(),*sid);
    if (!previous_user || !model->user() || *previous_user != *model->user()) {
        return InfUpdateFailure{InfUpdateError::identity_mismatch};
    }
    auto prepared = client.prepare_update(input);
    if (const auto* error = std::get_if<ClientSessionError>(&prepared)) { return InfUpdateFailure{*error}; }
    auto candidate = std::get<ClientInfo>(std::move(prepared));
    auto user = user_from_info(candidate,*sid);
    if (!user) { return InfUpdateFailure{InfUpdateError::invalid_public_user}; }

    Message delta{MessageType::broadcast,"INF",Command::inf,sid->str(),{},{},{},0};
    for (const auto& token : input.parameters) {
        const auto field = *parse_named_parameter(token); // Validated by prepare_update.
        if (const auto value = candidate.field(field.name)) {
            // Take normalized/validated data, never the untrusted input value.
            delta.parameters.emplace_back(std::string(field.name) + std::string(*value));
        } else if (client.info()->field(field.name)) {
            delta.parameters.emplace_back(field.name); // Explicit cancellation.
        }
    }
    std::optional<Message> public_delta;
    if (!delta.parameters.empty()) {
        if (!std::holds_alternative<std::string>(serialize(delta))) { return InfUpdateFailure{InfUpdateError::invalid_output}; }
        public_delta.emplace(std::move(delta));
    }
    // All projection/delta allocation precedes the registry's transactional
    // update. After it succeeds, ClientInfo/result moves cannot throw.
    if (const auto error = sessions_.update_user(id,std::move(*user),now)) {
        return std::visit([](auto value) -> InfUpdateResult { return InfUpdateFailure{value}; },*error);
    }
    client.commit_update(std::move(candidate));
    return AppliedInfUpdate{std::move(public_delta)};
}
} // namespace dc24h::adc
