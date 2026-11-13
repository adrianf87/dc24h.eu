#pragma once

#include "dc24h/users/connectivity.hpp"
#include "dc24h/users/session.hpp"

namespace dc24h::transfers {
enum class Kind;
struct Permissions {
    bool initiate{true}, receive{true};
    friend bool operator==(const Permissions&, const Permissions&) = default;
};
struct SecuritySettings {
    bool ctm_enabled{true}, rcm_enabled{true};
    Permissions guests{};
};
enum class PermissionError { disabled, forbidden };
[[nodiscard]] std::optional<PermissionError> authorize(const SecuritySettings& settings,
    Permissions sender, Permissions target, Kind kind) noexcept;
[[nodiscard]] constexpr bool valid_connection_port(std::int64_t port) noexcept { return port >= 1 && port <= 65535; }
// Borrowed, trusted owner-thread authority; must outlive Core. Lookups must be
// read-only, allocation-free and bound to the exact ready session lifetime.
// Missing grants fail closed. A null authority means the configured guest policy.
class PermissionSource {
public:
    virtual ~PermissionSource() = default;
    [[nodiscard]] virtual std::optional<Permissions> connection_permissions(users::SessionId id) const noexcept = 0;
};
} // namespace dc24h::transfers
