#pragma once
#include "dc24h/api/server.hpp"
#include "dc24h/database/database.hpp"
#include <memory>
namespace dc24h::configuration { class Review; }
namespace dc24h::api {
// Only for the separate, non-serving maintenance executable. Each call owns
// its SQL connection on the API worker; no password/SQL is logged or replayed.
[[nodiscard]] AccountHandler account_handler(database::Options options,
    std::shared_ptr<const configuration::Review> configuration_review = {});
// For the serving hub's PRIVATE AF_UNIX API only: password proof and grant
// discovery. No account writes, management actions, role changes or public
// network listeners. All other account routes are unconditionally denied.
[[nodiscard]] AccountHandler public_account_access_handler(database::Options options);
}
