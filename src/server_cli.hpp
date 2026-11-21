#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace dc24h {

void write_server_help(std::ostream &output);
int run_server(std::span<const std::string_view> arguments, std::ostream &output,
               std::ostream &errors);

} // namespace dc24h
