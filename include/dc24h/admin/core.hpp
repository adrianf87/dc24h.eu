#pragma once
#include "dc24h/accounts/management.hpp"
#include "dc24h/bans/repository.hpp"
#include <array>
#include <exception>
#include <optional>
#include <string_view>
#include <variant>

namespace dc24h::admin {
enum class Origin { maintenance_api, owner_cli };
enum class Action { create, activate, disable, password, reset, role, bootstrap_owner, transfer_owner, ban_create, ban_revoke };
inline constexpr std::array<Action,10> action_catalog{Action::create,Action::activate,Action::disable,
    Action::password,Action::reset,Action::role,Action::bootstrap_owner,Action::transfer_owner,Action::ban_create,Action::ban_revoke};
[[nodiscard]] std::string_view action_key(Action action) noexcept;
[[nodiscard]] std::optional<Action> parse_action(std::string_view key) noexcept;
// Entry-point availability only; never a replacement for account authorization.
[[nodiscard]] bool available(Action action, Origin origin) noexcept;
enum class ErrorKind { invalid_input, forbidden, stale, not_found, unavailable, outcome_unknown };
class Error final : public std::exception {
public:
    explicit Error(ErrorKind kind) noexcept : kind_(kind) {}
    [[nodiscard]] const char* what() const noexcept override;
    [[nodiscard]] ErrorKind kind() const noexcept { return kind_; }
private:
    ErrorKind kind_;
};
struct AccountRef { std::uint64_t id{}, revision{}; };
// Borrowed secrets live only for this synchronous call. Caller owns/erases them.
// Target revision is mandatory for existing-account mutations. Actor revision is
// optional for ordinary HTTP operations, mandatory for ownership transfer.
struct Command {
    Action action{};
    AccountRef actor{}, target{};
    std::string_view proof, password, name, successor_proof;
    accounts::Role role{accounts::Role::registered};
    accounts::Timestamp time{};
    bans::Kind ban_kind{bans::Kind::account};
    std::string_view subject,reason;
    std::optional<bans::Timestamp> expires_at;
};
using Result = std::variant<accounts::StoredAccount,accounts::OwnershipTransfer,bans::Record>;
struct Statistics {
    std::uint64_t attempts{}, succeeded{}, refused{}, unavailable{}, outcome_unknown{};
};
// Central synchronous maintenance dispatcher. Opens one owner-thread SQL
// connection per call; never runs on the ADC reactor. Serving hubs must be stopped.
// No secret, command, account ID or exception text is retained. No write replay,
// reconnect, hot grant, schema migration or implicit ownership entry point.
// A single caller/thread at a time owns an Engine, just like its HTTP worker.
class Engine final {
public:
    Engine(database::Options options, Origin origin);
    [[nodiscard]] Result execute(const Command& command);
    [[nodiscard]] accounts::StoredAccount account(std::uint64_t id);
    [[nodiscard]] std::vector<accounts::StoredAccount> list(std::uint64_t after = 0, bool staff = false);
    [[nodiscard]] std::vector<bans::Record> bans_list(std::uint64_t after=0);
    [[nodiscard]] Statistics statistics() const noexcept { return statistics_; }
    [[nodiscard]] Origin origin() const noexcept { return origin_; }
private:
    database::Options options_;
    Origin origin_;
    Statistics statistics_;
};
} // namespace dc24h::admin
