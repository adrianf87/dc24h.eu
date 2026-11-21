#pragma once

#include "dc24h/db/config.hpp"

#include <iosfwd>
#include <span>
#include <string_view>

namespace dc24h {

enum class DatabaseAccount { standard, api };
[[nodiscard]] db::Config read_database_config(DatabaseAccount account = DatabaseAccount::standard);
void write_database_help(std::ostream &output);
int run_database(std::span<const std::string_view> arguments, std::ostream &output,
                 std::ostream &errors);

} // namespace dc24h
