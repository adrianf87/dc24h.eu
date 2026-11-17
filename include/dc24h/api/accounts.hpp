#pragma once
#include "dc24h/api/server.hpp"
#include "dc24h/database/database.hpp"
namespace dc24h::api {
// Only for the separate, non-serving maintenance executable. Each call owns
// its SQL connection on the API worker; no password/SQL is logged or replayed.
[[nodiscard]] AccountHandler account_handler(database::Options options);
}
