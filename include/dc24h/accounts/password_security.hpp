#pragma once

#include "dc24h/accounts/account.hpp"
#include "dc24h/database/database.hpp"

#include <stdexcept>
#include <string_view>

namespace dc24h::accounts {
enum class PasswordVerification { matched, mismatch, unavailable };
enum class PasswordSecurityErrorKind { invalid_password, invalid_record, crypto, schema };
class PasswordSecurityError final : public std::runtime_error {
public:
    explicit PasswordSecurityError(PasswordSecurityErrorKind kind);
    [[nodiscard]] PasswordSecurityErrorKind kind() const noexcept { return kind_; }
private:
    PasswordSecurityErrorKind kind_;
};

// Trusted synchronous credential check, not a login or permission grant.
// Borrows its connection on the owning thread; refuses caller transactions.
// Caller authorizes access, bounds concurrent KDF work, owns/clears plaintext
// and keeps it alive throughout the call. No credential material is returned.
// A match concerns one stored verifier snapshot, including disabled accounts;
// account status, live identity, session admission and authorization are separate.
class PasswordSecurityService final {
public:
    explicit PasswordSecurityService(database::Connection& connection) noexcept : connection_(connection) {}
    [[nodiscard]] PasswordVerification verify(AccountId account, std::string_view password);
private:
    database::Connection& connection_;
};
} // namespace dc24h::accounts
