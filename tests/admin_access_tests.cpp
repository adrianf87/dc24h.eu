#include "dc24h/admin/access.hpp"
#include "dc24h/security/roles.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace dc24h;

namespace {
int failures = 0;
void require(bool condition) {
    if (!condition) { ++failures; }
}
bool fact(accounts::Role role, std::string_view key, bool granted, bool implemented) {
    for (const auto& item : admin::permission_facts(role)) {
        if (item.key == key) { return item.granted == granted && item.implemented == implemented; }
    }
    return false;
}
}

int main() {
    using accounts::Role;
    require(admin::permission_facts(Role::administrator).size() == security::permission_catalog.size());
    require(admin::access_routes.size() == 12);
    require(fact(Role::administrator, "configuration.manage", true, false));
    require(fact(Role::administrator, "hub.manage", false, false));
    require(fact(Role::owner, "hub.manage", true, false));
    require(fact(Role::owner, "configuration.manage", true, false));
    require(fact(Role::registered, "accounts.password.change", true, true));
    require(fact(Role::registered, "accounts.manage", false, true));
    require(admin::route_allowed(Role::registered, "GET", "/api/v1/admin"));
    require(admin::route_allowed(Role::registered, "POST", "/api/v1/accounts/1/password"));
    require(!admin::route_allowed(Role::registered, "GET", "/api/v1/accounts"));
    require(!admin::route_allowed(Role::registered, "GET", "/api/v1/accounts?staff_after=0"));
    require(!admin::route_allowed(Role::registered, "POST", "/api/v1/audit"));
    require(!admin::route_allowed(Role::operator_, "GET", "/api/v1/accounts?staff_after=0"));
    require(admin::route_allowed(Role::operator_, "GET", "/api/v1/bans"));
    require(admin::route_allowed(Role::operator_, "POST", "/api/v1/bans/9/revoke"));
    require(!admin::route_allowed(Role::moderator, "GET", "/api/v1/bans?state=active"));
    require(admin::route_allowed(Role::moderator, "POST", "/api/v1/audit"));
    require(admin::route_allowed(Role::administrator, "POST", "/api/v1/accounts/12/role"));
    require(admin::route_allowed(Role::administrator, "GET", "/api/v1/accounts?staff_after=0"));
    require(admin::route_allowed(Role::registered, "POST", "/api/v1/accounts/1/unknown"));
    require(!admin::route_allowed(Role::administrator, "GET", "/api/v1/status"));
    require(!admin::route_allowed(Role::administrator, "POST", "/api/v1/admin/access"));
    for (const auto& route : admin::access_routes) {
        require(route.permission != "configuration.manage" && route.permission != "hub.manage");
        require(admin::catalog_allowed(Role::owner, route.permission));
    }
    require(admin::catalog_allowed(Role::registered, ""));
    require(!admin::catalog_allowed(Role::operator_, "accounts.manage"));
    require(admin::catalog_allowed(Role::operator_, "users.ban+blocks.manage"));
    require(!admin::catalog_allowed(Role::moderator, "users.ban+blocks.manage"));
    require(admin::catalog_allowed(Role::registered, "accounts.password.change"));
    if (failures) {
        std::cerr << failures << " admin access checks failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
