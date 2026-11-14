#pragma once
#include "dc24h/database/database.hpp"
#include <span>

namespace dc24h::database::detail {
struct Entry { MigrationInfo info; std::span<const std::string_view> statements; };
// Private seam for testing future ordered, multi-statement catalogs and failures.
void validate_catalog(std::span<const Entry> entries);
MigrationStatus inspect(Connection& connection, std::span<const Entry> entries);
unsigned int run(Connection& connection, std::span<const Entry> entries);
}
