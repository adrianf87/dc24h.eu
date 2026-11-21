#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace dc24h {

int run_cli(std::span<const std::string_view> arguments, std::ostream &output,
            std::ostream &errors);

} // namespace dc24h
