#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace dc24h::accounts {
// Persistent account identity, independent of connection SID/CID and SessionId.
// Restricted to the positive signed BIGINT range for future SQL adapters.
class AccountId final {
public:
    [[nodiscard]] static std::optional<AccountId> from_value(std::uint64_t value) noexcept;
    [[nodiscard]] std::uint64_t value() const noexcept { return value_; }
    friend bool operator==(const AccountId&, const AccountId&) = default;
private:
    explicit AccountId(std::uint64_t value) noexcept : value_(value) {}
    std::uint64_t value_;
};

inline constexpr std::size_t max_account_name_bytes = 64;
// Owning already-NFC UTF-8, without control/format/separator characters.
// Exact bytes and case are preserved; this is not a uniqueness registry.
class AccountName final {
public:
    AccountName(const AccountName&) = default;
    AccountName(AccountName&& other) noexcept;
    AccountName& operator=(AccountName other) noexcept;
    [[nodiscard]] static std::optional<AccountName> create(std::string_view value);
    [[nodiscard]] const std::string& text() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept;
    friend bool operator==(const AccountName&, const AccountName&) = default;
private:
    explicit AccountName(std::string_view value) : value_(value) {}
    std::string value_;
};

enum class Role { registered, moderator, administrator };
enum class Status { disabled, active };
[[nodiscard]] std::string_view role_key(Role role) noexcept;
[[nodiscard]] std::string_view status_key(Status status) noexcept;
[[nodiscard]] std::optional<Role> parse_role(std::string_view key) noexcept;
[[nodiscard]] std::optional<Status> parse_status(std::string_view key) noexcept;

// UTC Unix seconds, never local time or a steady-clock duration. Epoch is valid;
// the upper bound is 9999-12-31T23:59:59Z. No dependency on the current clock.
using Timestamp = std::int64_t;
inline constexpr Timestamp max_timestamp = 253402300799LL;
struct Times {
    Timestamp created_at{}, updated_at{};
    std::optional<Timestamp> last_login_at;
    friend bool operator==(const Times&, const Times&) = default;
};

// Untrusted owning import/snapshot data. Defaults do not identify or enable an
// account. Credentials, PID/CID/SID, IPs and public INF profile are excluded.
struct AccountData {
    std::uint64_t id{};
    std::string name;
    Role role{Role::registered};
    Status status{Status::disabled};
    Times times;
    friend bool operator==(const AccountData&, const AccountData&) = default;
};
enum class AccountError { invalid_id, invalid_name, invalid_role, invalid_status, invalid_times };

// Checked detached metadata, not authentication evidence or a permission grant.
// Updates create a new candidate; callers commit/persist it separately.
class Account final {
public:
    Account(const Account&) = default;
    Account(Account&&) noexcept = default;
    Account& operator=(Account other) noexcept;
    [[nodiscard]] static std::variant<Account, AccountError> create(const AccountData& data);
    [[nodiscard]] AccountId id() const noexcept { return id_; }
    [[nodiscard]] const AccountName& name() const noexcept { return name_; }
    [[nodiscard]] Role role() const noexcept { return role_; }
    [[nodiscard]] Status status() const noexcept { return status_; }
    [[nodiscard]] const Times& times() const noexcept { return times_; }
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] AccountData snapshot() const;
    [[nodiscard]] std::variant<Account, AccountError> with_name(
        std::string_view name, Timestamp changed_at) const;
    [[nodiscard]] std::variant<Account, AccountError> with_access(
        Role role, Status status, Timestamp changed_at) const;
    // Historical metadata only: this method does not check credentials or admit
    // a connection, even when the account status is active.
    [[nodiscard]] std::variant<Account, AccountError> with_last_login(Timestamp logged_at) const;
    friend bool operator==(const Account&, const Account&) = default;
private:
    Account(AccountId id, AccountName name, Role role, Status status, Times times);
    [[nodiscard]] std::optional<AccountError> error() const noexcept;
    AccountId id_;
    AccountName name_;
    Role role_;
    Status status_;
    Times times_;
};
} // namespace dc24h::accounts
