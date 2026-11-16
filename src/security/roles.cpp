#include "dc24h/security/roles.hpp"

namespace dc24h::security {
bool allows(accounts::Role role, Permission permission) noexcept {
    using accounts::Role;
    switch (role) {
    case Role::registered: case Role::moderator: case Role::administrator: break;
    default: return false;
    }
    switch (permission) {
    case Permission::search_queries: case Permission::search_results:
    case Permission::initiate_transfers: case Permission::receive_transfers: return true;
    case Permission::revoke_sessions: return role == Role::moderator || role == Role::administrator;
    }
    return false;
}
bool can_revoke(accounts::Role actor, accounts::Role target) noexcept {
    using accounts::Role;
    if (!allows(actor,Permission::revoke_sessions)) { return false; }
    return target == Role::registered || (actor == Role::administrator && target == Role::moderator);
}
} // namespace dc24h::security
