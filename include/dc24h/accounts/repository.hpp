#pragma once

#include "dc24h/accounts/account.hpp"
#include "dc24h/database/database.hpp"

#include <stdexcept>

namespace dc24h::accounts {
// No credentials or live-session permissions. ID and revision come from SQL.
struct NewAccount {
    std::string name;
    Role role{Role::registered};
    Status status{Status::disabled};
    Times times{};
};
struct StoredAccount {
    Account account;
    std::uint64_t revision;
    friend bool operator==(const StoredAccount&, const StoredAccount&) = default;
};
enum class RepositoryErrorKind { invalid_account, duplicate, stale, invalid_record, schema };
class RepositoryError final : public std::runtime_error {
public:
    explicit RepositoryError(RepositoryErrorKind kind);
    [[nodiscard]] RepositoryErrorKind kind() const noexcept { return kind_; }
private:
    RepositoryErrorKind kind_;
};

// Borrowed connection must outlive this adapter and stay on its owning thread.
// Synchronous: keep off the Network reactor. Requires explicit migrations;
// refuses caller-owned transactions. Each write owns a rollback-on-exit scope.
// Database errors retain outcome_unknown; never automatically retry writes.
class AccountRepository final {
public:
    explicit AccountRepository(database::Connection& connection) noexcept : connection_(connection) {}
    [[nodiscard]] std::optional<StoredAccount> find_by_id(AccountId id);
    [[nodiscard]] std::optional<StoredAccount> find_by_name(const AccountName& name);
    [[nodiscard]] StoredAccount insert(const NewAccount& account);
    // Exact expected snapshot and revision must still match. Preserves identity
    // and creation time; update/login times cannot go backwards or lose login.
    // Missing or superseded rows report stale, including same-second changes.
    [[nodiscard]] StoredAccount update(const StoredAccount& expected, const Account& replacement);
private:
    friend class RegistrationService;
    friend class IdentityRepository;
    friend class PasswordSecurityService;
    // Internal composition point; caller owns rollback and commit. Never public.
    [[nodiscard]] StoredAccount insert_in_transaction(const NewAccount& account);
    void require_schema();
    database::Connection& connection_;
};
} // namespace dc24h::accounts
