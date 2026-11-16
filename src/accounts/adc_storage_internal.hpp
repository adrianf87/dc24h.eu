#pragma once
#include "dc24h/database/database.hpp"
namespace dc24h::accounts::detail {
// Internal checked schema composition; always outside a caller transaction.
void require_adc_credentials_schema(database::Connection& connection);
}
