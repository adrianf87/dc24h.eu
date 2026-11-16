#pragma once

#include "dc24h/accounts/repository.hpp"
#include <string_view>

namespace dc24h::accounts {
enum class ManagementErrorKind { invalid_input, invalid_password, unauthorized, stale, invalid_record, crypto, schema };
class ManagementError final : public std::runtime_error {
public:
    explicit ManagementError(ManagementErrorKind kind);
    [[nodiscard]] ManagementErrorKind kind() const noexcept { return kind_; }
private:
    ManagementErrorKind kind_;
};
// Trusted synchronous maintenance API, never a reactor/public HTTP operation.
// Stop the hub before writes: startup directories retain old secrets/access.
// Each operation atomically increments the target revision and deletes its ADC
// credential. Re-enroll a separate ADC password and restart before serving again.
// Borrowed plaintext stays caller-owned; the service wipes its verifier buffers.
// No migration, automatic retry, role change, live grant or secret in the result.
class AccountManagementService final {
public:
    explicit AccountManagementService(database::Connection& connection) noexcept : connection_(connection) {}
    [[nodiscard]] StoredAccount change_password(const StoredAccount& expected,
        std::string_view current_password, std::string_view new_password, Timestamp changed_at);
    // Fresh active administrator metadata + password proof; only registered or
    // moderator targets, never self/administrator. Reset leaves target disabled.
    [[nodiscard]] StoredAccount disable_account(const StoredAccount& actor,
        std::string_view actor_password, const StoredAccount& expected, Timestamp changed_at);
    [[nodiscard]] StoredAccount reset_password(const StoredAccount& actor,
        std::string_view actor_password, const StoredAccount& expected,
        std::string_view new_password, Timestamp changed_at);
private:
    enum class Action { change, disable, reset };
    [[nodiscard]] StoredAccount perform(Action action, const StoredAccount& actor,
        std::string_view proof, const StoredAccount& target, std::string_view replacement, Timestamp changed_at);
    database::Connection& connection_;
};
} // namespace dc24h::accounts
