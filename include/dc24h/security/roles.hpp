#pragma once
#include "dc24h/accounts/account.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace dc24h::security {
// Existing enumerators remain source compatible; names on the wire/config are
// explicit catalog keys. Unknown roles, permissions and masks fail closed.
enum class Permission { search_queries, search_results, initiate_transfers, receive_transfers,
    revoke_sessions, manage_accounts, hub_use, public_chat, private_chat, mute_users,
    ban_users, manage_blocks, manage_roles, read_audit, manage_configuration, manage_hub, change_own_password };
inline constexpr std::array<Permission,17> permission_catalog{
    Permission::hub_use,Permission::public_chat,Permission::private_chat,
    Permission::search_queries,Permission::search_results,Permission::initiate_transfers,
    Permission::receive_transfers,Permission::revoke_sessions,Permission::mute_users,
    Permission::ban_users,Permission::manage_blocks,Permission::manage_accounts,
    Permission::manage_roles,Permission::read_audit,Permission::manage_configuration,Permission::manage_hub,Permission::change_own_password};
struct RolePolicy { std::uint32_t vip_permissions{}; };
[[nodiscard]] std::string_view permission_key(Permission permission) noexcept;
[[nodiscard]] std::optional<Permission> parse_permission(std::string_view key) noexcept;
[[nodiscard]] std::optional<unsigned> role_rank(accounts::Role role) noexcept;
// Whether this increment provides an enforcing engine/service, rather than only
// reserving a capability. Runtime global limits and endpoint availability still apply.
[[nodiscard]] bool permission_implemented(Permission permission) noexcept;
[[nodiscard]] RolePolicy parse_vip_permissions(std::string_view text);
void validate(const RolePolicy& policy);
[[nodiscard]] bool allows(accounts::Role role, Permission permission, RolePolicy policy = {}) noexcept;
[[nodiscard]] bool can_revoke(accounts::Role actor, accounts::Role target, RolePolicy policy = {}) noexcept;
[[nodiscard]] bool can_manage(accounts::Role actor, accounts::Role target) noexcept;
// No self/equal/higher-role changes or assignment at/above the actor's rank.
// Guest has no account; Owner bootstrap/ownership transfer stays trusted/offline.
[[nodiscard]] bool can_assign(accounts::Role actor, accounts::Role target, accounts::Role proposed) noexcept;
}
