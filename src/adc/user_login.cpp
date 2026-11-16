#include "dc24h/adc/user_login.hpp"
#include "dc24h/adc/user.hpp"

#include <type_traits>
#include <utility>

namespace dc24h::adc {
namespace {
UserLoginResult promote(users::MutationResult result) noexcept {
    if (!result) { return {}; }
    return std::visit([](auto error) -> UserLoginResult { return error; }, *result);
}
bool verified(const ClientSession& client) noexcept {
    if (!client.info()) { return false; }
    const auto identity = client.info()->identity();
    const auto cid = client.info()->field("ID");
    return identity && cid && TigerCid::parse(*cid) == identity->cid() && client.info()->field("NI");
}
}
std::string_view user_login_status(const UserLoginFailure& error) noexcept {
    return std::visit([](auto value) -> std::string_view {
        using T = decltype(value);
        if constexpr (std::is_same_v<T, users::RegistryError>) {
            switch (value) {
            case users::RegistryError::duplicate_cid: return "224";
            case users::RegistryError::duplicate_nickname: return "222";
            case users::RegistryError::capacity_reached: return "211";
            case users::RegistryError::user_capacity_reached: return "211";
            default: return "210";
            }
        } else if constexpr (std::is_same_v<T, users::SessionError>) {
            switch (value) {
            case users::SessionError::invalid_state: return "244";
            case users::SessionError::invalid_identity: return "243";
            default: return "210";
            }
        } else {
            switch (value) {
            case UserLoginError::invalid_protocol_state: return "244";
            case UserLoginError::unverified_identity: return "227";
            case UserLoginError::identity_mismatch: return "240";
            }
        }
        return "210";
    }, error);
}
UserLoginResult UserLogin::register_guest(users::SessionId id, const ClientSession& client, users::TimePoint now) {
    return register_approved(id,client,now);
}
UserLoginResult UserLogin::register_approved(users::SessionId id, const ClientSession& client, users::TimePoint now) {
    const auto* model = sessions_.find(id);
    if (!model) { return users::RegistryError::missing; }
    if (model->phase() != users::Phase::negotiating) { return users::SessionError::invalid_state; }
    // NORMAL records explicit guest approval or successful password approval;
    // it does not mean the welcome has been sent or the managed user is ready.
    if (client.state() != ConnectionState::normal) { return UserLoginError::invalid_protocol_state; }
    if (!verified(client)) { return UserLoginError::unverified_identity; }
    const auto sid = Sid::parse(client.sid());
    if (!sid || sid->value() == 0) { return UserLoginError::unverified_identity; }
    auto user = user_from_info(*client.info(), *sid);
    if (!user) { return UserLoginError::unverified_identity; }
    // Projection allocates before mutation. SessionManager stages all public
    // indexes transactionally; failure leaves the model and registry unchanged.
    return promote(sessions_.identify(id, std::move(*user), now));
}
UserLoginResult UserLogin::complete_welcome(users::SessionId id, const ClientSession& client, users::TimePoint now) noexcept {
    const auto* model = sessions_.find(id);
    if (!model) { return users::RegistryError::missing; }
    if (model->phase() != users::Phase::identified) { return users::SessionError::invalid_state; }
    if (client.state() != ConnectionState::normal) { return UserLoginError::invalid_protocol_state; }
    if (!verified(client)) { return UserLoginError::unverified_identity; }
    const auto* identity = model->identity();
    if (!identity || identity->sid() != client.sid() ||
        client.info()->field("ID") != identity->cid() ||
        client.info()->field("NI") != identity->nickname()) { return UserLoginError::identity_mismatch; }
    return promote(sessions_.mark_ready(id, now));
}
} // namespace dc24h::adc
