#pragma once

#include "dc24h/accounts/account.hpp"
#include "dc24h/database/database.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dc24h::admin {

// In-memory proof that an active account password was accepted by this
// process. Four grants, 600 seconds on the monotonic clock. Restart drops
// them. They are not written to SQL and do not add an audit row.
// Each slot also holds a public session id and a separate CSRF secret.
// allows() is evaluated with the default role table. A configured VIP mask
// is not applied to a grant. configuration.manage and hub.manage stay
// unimplemented. POST /api/v1/management/authorize checks the live grant and
// does not execute the operation. That contract is not one of the twelve routes.
// beta-0.1.2 separately gates exact GET/POST /management/configuration for
// read-only maintenance review and in-memory validation; no writes are enabled.
inline constexpr std::size_t access_grant_limit = 4;
inline constexpr std::chrono::seconds access_grant_ttl{600};
// Requests that present one live grant. The window is not derived from a
// client address or a proxy header. The process POST gap is separate.
inline constexpr std::uint32_t session_request_limit = 24;
inline constexpr std::chrono::seconds session_request_window{10};

struct IssuedAccess {
    std::uint64_t id{};
    std::string role;
    std::string status;
    std::string grant;
    std::string session;
    std::string csrf;
};

enum class GrantCheck {
    accepted, missing, forbidden, not_found, unavailable, outcome_unknown, csrf_required, rate_limited
};

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
    // Every call requires the session id and CSRF secret from the same slot.
    // The password proof does not call authorize. A full session window is rate_limited.
    [[nodiscard]] GrantCheck authorize(std::string_view token, std::string_view method, std::string_view target,
        std::string_view session, std::string_view csrf);
    // Reloads the actor after a live grant was accepted. A missing grant, a
    // missing row or an inactive account returns nullopt. Database failures throw.
    [[nodiscard]] std::optional<accounts::Role> actor_role(std::string_view token);
    // Call only after authorize() accepted the same request. The id is
    // matched to the in-memory grant; it is never taken from the request body.
    [[nodiscard]] std::optional<std::uint64_t> authorized_actor(std::string_view token) const noexcept;

private:
    struct Slot {
        bool used{false};
        std::array<unsigned char, 32> secret{};
        std::array<unsigned char, 32> csrf{};
        std::array<unsigned char, 16> session{};
        std::uint64_t actor{};
        std::chrono::steady_clock::time_point expires{};
        std::chrono::steady_clock::time_point window_started{};
        std::uint32_t window_count{};
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
