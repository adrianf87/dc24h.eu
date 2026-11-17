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
// Existing-account operations atomically increment the target revision and delete
// its ADC credential. Creation inserts a new disabled account and verifier.
// Re-enroll a separate ADC password and restart before serving again.
// Borrowed plaintext stays caller-owned; the service wipes its verifier buffers.
// No migration, automatic retry, live grant or secret in the result.
class AccountManagementService final {
public:
    explicit AccountManagementService(database::Connection& connection) noexcept : connection_(connection) {}
    // Administrator-proven disabled registration, with authority and exact
    // credential rechecked under the account lock before inserting either row.
    [[nodiscard]] StoredAccount create_account(const StoredAccount& actor,
        std::string_view actor_password, std::string_view name, std::string_view password, Timestamp created_at);
    [[nodiscard]] StoredAccount change_password(const StoredAccount& expected,
        std::string_view current_password, std::string_view new_password, Timestamp changed_at);
    // Fresh active administrative authority + password proof; strictly lower
    // targets only, never self/equal/higher. Reset leaves the target disabled.
    [[nodiscard]] StoredAccount disable_account(const StoredAccount& actor,
        std::string_view actor_password, const StoredAccount& expected, Timestamp changed_at);
    [[nodiscard]] StoredAccount reset_password(const StoredAccount& actor,
        std::string_view actor_password, const StoredAccount& expected,
        std::string_view new_password, Timestamp changed_at);
    [[nodiscard]] StoredAccount activate_account(const StoredAccount& actor,
        std::string_view actor_password, const StoredAccount& expected, Timestamp changed_at);
    // Stronger actor and strictly lower proposed rank. Disables the target and
    // removes ADC credentials; ownership bootstrap/transfer is not this operation.
    [[nodiscard]] StoredAccount change_role(const StoredAccount& actor,
        std::string_view actor_password, const StoredAccount& expected, Role role, Timestamp changed_at);
private:
    enum class Action { change, disable, reset, activate, role };
    [[nodiscard]] StoredAccount perform(Action action, const StoredAccount& actor,
        std::string_view proof, const StoredAccount& target, std::string_view replacement, Timestamp changed_at,
        Role role = Role::registered);
    database::Connection& connection_;
};
} // namespace dc24h::accounts
