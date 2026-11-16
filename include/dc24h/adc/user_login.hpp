#pragma once

#include "dc24h/adc/inf.hpp"
#include "dc24h/users/session_manager.hpp"

namespace dc24h::adc {
enum class UserLoginError { invalid_protocol_state, unverified_identity, identity_mismatch };
using UserLoginFailure = std::variant<UserLoginError, users::RegistryError, users::SessionError>;
using UserLoginResult = std::optional<UserLoginFailure>;
[[nodiscard]] std::string_view user_login_status(const UserLoginFailure& error) noexcept;

// One reactor/thread. Bridge from an already validated and policy-approved ADC
// ClientSession to the managed public User. Owns no credentials or user copies.
// LoginHub stages bounded welcome output before registration, and calls
// complete_welcome only after the final LF of the own-INF is actually sent.
class UserLogin final {
public:
    explicit UserLogin(users::SessionManager& sessions) noexcept : sessions_(sessions) {}
    UserLogin(const UserLogin&) = delete;
    UserLogin& operator=(const UserLogin&) = delete;
    UserLogin(UserLogin&&) = delete;
    UserLogin& operator=(UserLogin&&) = delete;
    [[nodiscard]] UserLoginResult register_guest(users::SessionId id, const ClientSession& client,
        users::TimePoint now);
    [[nodiscard]] UserLoginResult register_approved(users::SessionId id, const ClientSession& client,
        users::TimePoint now);
    [[nodiscard]] UserLoginResult complete_welcome(users::SessionId id, const ClientSession& client,
        users::TimePoint now, std::optional<accounts::AccountId> account = {}) noexcept;
private:
    users::SessionManager& sessions_;
};
} // namespace dc24h::adc
