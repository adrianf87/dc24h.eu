#include "dc24h/accounts/account.hpp"
#include "dc24h/text/public_text.hpp"

#include <utf8proc.h>

#include <limits>
#include <utility>

namespace dc24h::accounts {
namespace {
bool timestamp(Timestamp value) noexcept { return value >= 0 && value <= max_timestamp; }
bool valid_times(const Times& times) noexcept {
    return timestamp(times.created_at) && timestamp(times.updated_at) &&
        times.updated_at >= times.created_at && (!times.last_login_at ||
        (timestamp(*times.last_login_at) && *times.last_login_at >= times.created_at &&
            *times.last_login_at <= times.updated_at));
}
}
std::optional<AccountId> AccountId::from_value(std::uint64_t value) noexcept {
    if (value == 0 || value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    return AccountId(value);
}
AccountName::AccountName(AccountName&& other) noexcept : value_(std::move(other.value_)) {
    other.value_.clear();
}
AccountName& AccountName::operator=(AccountName other) noexcept {
    value_.swap(other.value_);
    return *this;
}
bool AccountName::valid() const noexcept {
    // Only construction and moves can change the bytes. The full UTF-8 policy
    // is checked on construction; a moved-from string can be empty.
    return !value_.empty() && value_.size() <= max_account_name_bytes;
}
std::optional<AccountName> AccountName::create(std::string_view value) {
    if (value.empty() || value.size() > max_account_name_bytes || !text::valid_public_text(value, true)) {
        return std::nullopt;
    }
    std::size_t offset = 0;
    while (offset < value.size()) {
        utf8proc_int32_t codepoint = 0;
        const auto count = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(value.data() + offset),
            static_cast<utf8proc_ssize_t>(value.size() - offset), &codepoint);
        if (count <= 0 || utf8proc_category(codepoint) == UTF8PROC_CATEGORY_ZS) { return std::nullopt; }
        offset += static_cast<std::size_t>(count);
    }
    return AccountName(value);
}
std::string_view role_key(Role role) noexcept {
    switch (role) {
        case Role::registered: return "registered";
        case Role::moderator: return "moderator";
        case Role::administrator: return "administrator";
        case Role::vip: return "vip";
        case Role::operator_: return "operator";
        case Role::owner: return "owner";
        case Role::guest: return "guest";
    }
    return {};
}
std::string_view status_key(Status status) noexcept {
    switch (status) {
        case Status::disabled: return "disabled";
        case Status::active: return "active";
    }
    return {};
}
std::optional<Role> parse_role(std::string_view key) noexcept {
    for (const auto role : role_catalog) {
        if (role_key(role) == key) { return role; }
    }
    return std::nullopt;
}
bool account_role(Role role) noexcept { return role!=Role::guest&&!role_key(role).empty(); }
std::optional<Status> parse_status(std::string_view key) noexcept {
    for (const auto status : {Status::disabled, Status::active}) {
        if (status_key(status) == key) { return status; }
    }
    return std::nullopt;
}
Account::Account(AccountId id, AccountName name, Role role, Status status, Times times)
    : id_(id), name_(std::move(name)), role_(role), status_(status), times_(times) {}
Account& Account::operator=(Account other) noexcept {
    using std::swap;
    swap(id_, other.id_); swap(name_, other.name_); swap(role_, other.role_);
    swap(status_, other.status_); swap(times_, other.times_);
    return *this;
}
std::optional<AccountError> Account::error() const noexcept {
    if (!AccountId::from_value(id_.value())) { return AccountError::invalid_id; }
    if (!name_.valid()) { return AccountError::invalid_name; }
    if (!account_role(role_)) { return AccountError::invalid_role; }
    if (status_key(status_).empty()) { return AccountError::invalid_status; }
    if (!valid_times(times_)) { return AccountError::invalid_times; }
    return std::nullopt;
}
bool Account::valid() const noexcept { return !error(); }
std::variant<Account, AccountError> Account::create(const AccountData& data) {
    const auto id = AccountId::from_value(data.id);
    if (!id) { return AccountError::invalid_id; }
    // Reject cheap invalid metadata before performing text normalization/copies.
    if (!account_role(data.role)) { return AccountError::invalid_role; }
    if (status_key(data.status).empty()) { return AccountError::invalid_status; }
    if (!valid_times(data.times)) { return AccountError::invalid_times; }
    auto name = AccountName::create(data.name);
    if (!name) { return AccountError::invalid_name; }
    return Account(*id, std::move(*name), data.role, data.status, data.times);
}
AccountData Account::snapshot() const {
    return {id_.value(), name_.text(), role_, status_, times_};
}
std::variant<Account, AccountError> Account::with_name(std::string_view value, Timestamp changed_at) const {
    if (const auto failure = error()) { return *failure; }
    if (!timestamp(changed_at) || changed_at < times_.updated_at) { return AccountError::invalid_times; }
    auto name = AccountName::create(value);
    if (!name) { return AccountError::invalid_name; }
    auto times = times_; times.updated_at = changed_at;
    return Account(id_, std::move(*name), role_, status_, times);
}
std::variant<Account, AccountError> Account::with_access(Role role, Status status, Timestamp changed_at) const {
    if (const auto failure = error()) { return *failure; }
    if (!account_role(role)) { return AccountError::invalid_role; }
    if (status_key(status).empty()) { return AccountError::invalid_status; }
    if (!timestamp(changed_at) || changed_at < times_.updated_at) { return AccountError::invalid_times; }
    auto times = times_; times.updated_at = changed_at;
    return Account(id_, name_, role, status, times);
}
std::variant<Account, AccountError> Account::with_last_login(Timestamp logged_at) const {
    if (const auto failure = error()) { return *failure; }
    if (!timestamp(logged_at) || logged_at < times_.updated_at) { return AccountError::invalid_times; }
    auto times = times_; times.last_login_at = logged_at; times.updated_at = logged_at;
    return Account(id_, name_, role_, status_, times);
}
} // namespace dc24h::accounts
