#pragma once

#include "dc24h/accounts/repository.hpp"

#include <string_view>

namespace dc24h::accounts {
inline constexpr std::size_t min_password_characters = 15;
inline constexpr std::size_t max_registration_password_bytes = 1024;
enum class RegistrationErrorKind { invalid_name, invalid_password, invalid_time, crypto, schema };
class RegistrationError final : public std::runtime_error {
public:
    explicit RegistrationError(RegistrationErrorKind kind);
    [[nodiscard]] RegistrationErrorKind kind() const noexcept { return kind_; }
private:
    RegistrationErrorKind kind_;
};

// Trusted synchronous application API, not a public registration endpoint.
// Borrowed connection/password must outlive the call and remain on the owner
// thread. Caller owns and erases its plaintext. No password/hash in the result.
// Accounts are always registered/disabled, with no last login or session grant.
// Explicit UTC time; no auto-migration, reconnect or write retry.
class RegistrationService final {
public:
    explicit RegistrationService(database::Connection& connection) noexcept : connection_(connection) {}
    [[nodiscard]] StoredAccount create_account(
        std::string_view name, std::string_view password, Timestamp created_at);
private:
    void require_credentials_schema();
    database::Connection& connection_;
};
} // namespace dc24h::accounts
