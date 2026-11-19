#pragma once

#include "dc24h/accounts/account.hpp"
#include "dc24h/database/database.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace dc24h::admin {

// In-memory proof that an active account password was accepted by this
// process. Four grants, 600 seconds on the monotonic clock. Restart drops
// them. They are not written to SQL and do not add an audit row.
// allows() is evaluated with the default role table. A configured VIP mask
// is not applied to a grant. configuration.manage and hub.manage stay
// unimplemented and have no route.
inline constexpr std::size_t access_grant_limit = 4;
inline constexpr std::chrono::seconds access_grant_ttl{600};

struct IssuedAccess {
    std::uint64_t id{};
    std::string role;
    std::string status;
    std::string grant;
};

enum class GrantCheck { accepted, missing, forbidden, not_found, unavailable, outcome_unknown };

struct PermissionFact {
    std::string_view key;
    bool granted{};
    bool implemented{};
};
struct RouteTemplate {
    std::string_view method;
    std::string_view path;
    std::string_view permission;
};

inline constexpr std::array<RouteTemplate, 12> access_routes{{
    {"GET", "/api/v1/admin", ""},
    {"GET", "/api/v1/accounts", "accounts.manage"},
    {"POST", "/api/v1/accounts", "accounts.manage"},
    {"POST", "/api/v1/accounts/{id}/activate", "accounts.manage"},
    {"POST", "/api/v1/accounts/{id}/disable", "accounts.manage"},
    {"POST", "/api/v1/accounts/{id}/password", "accounts.password.change"},
    {"POST", "/api/v1/accounts/{id}/reset", "accounts.manage"},
    {"POST", "/api/v1/accounts/{id}/role", "roles.manage"},
    {"GET", "/api/v1/bans", "users.ban+blocks.manage"},
    {"POST", "/api/v1/bans", "users.ban+blocks.manage"},
    {"POST", "/api/v1/bans/{id}/revoke", "users.ban+blocks.manage"},
    {"POST", "/api/v1/audit", "audit.read"},
}};

[[nodiscard]] std::array<PermissionFact, 17> permission_facts(accounts::Role role) noexcept;
[[nodiscard]] bool catalog_allowed(accounts::Role role, std::string_view permission) noexcept;
// Concrete maintenance target. Unknown account mutations are allowed for any
// role so the existing handler can answer 404. Paths outside maintenance are false.
[[nodiscard]] bool route_allowed(accounts::Role role, std::string_view method, std::string_view target) noexcept;

// One API worker thread owns a gate. A new proof for the same actor replaces
// that actor's grant. A full table replaces the soonest expiry.
class AccessGate final {
public:
    explicit AccessGate(database::Options options);
    // Active account required. Inactive accounts are refused before the password
    // check. Empty and overlong proofs are rejected before SQL.
    [[nodiscard]] IssuedAccess issue(std::uint64_t actor, std::string_view proof);
    // Reloads the actor. A missing, expired or unknown token is missing.
    // A live account that no longer holds the route permission is forbidden.
    [[nodiscard]] GrantCheck authorize(std::string_view token, std::string_view method, std::string_view target);

private:
    struct Slot {
        bool used{false};
        std::array<unsigned char, 32> secret{};
        std::uint64_t actor{};
        std::chrono::steady_clock::time_point expires{};
        Slot() = default;
        // std::function copies the maintenance handler once while the table is empty.
        // A later copy keeps its own secret bytes; destruction cleanses only that copy.
        Slot(const Slot& other) noexcept;
        Slot& operator=(const Slot& other) noexcept;
        Slot(Slot&& other) noexcept;
        Slot& operator=(Slot&& other) noexcept;
        ~Slot();
        void clear() noexcept;
    };
    database::Options options_;
    std::array<Slot, access_grant_limit> slots_{};
    [[nodiscard]] std::size_t choose_slot(std::uint64_t actor) const noexcept;
};

} // namespace dc24h::admin
