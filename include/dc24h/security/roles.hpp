#pragma once

#include "dc24h/accounts/account.hpp"

namespace dc24h::security {
// Account roles have explicit capabilities. Guest access remains configured
// separately; unknown role/permission values fail closed. No I/O or clocks.
enum class Permission { search_queries, search_results, initiate_transfers, receive_transfers, revoke_sessions };
[[nodiscard]] bool allows(accounts::Role role, Permission permission) noexcept;
// Staff can revoke only lower roles: moderator -> registered;
// administrator -> registered/moderator. Equal roles and unknown values deny.
[[nodiscard]] bool can_revoke(accounts::Role actor, accounts::Role target) noexcept;
} // namespace dc24h::security
