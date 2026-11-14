#pragma once
#include "dc24h/adc/connection_routing.hpp"

namespace dc24h::adc {
enum class FailureAction { reply, terminate, abort };
struct ConnectionFailure {
    FailureAction action;
    std::string_view code, description;
};
// Bounded local descriptions only; never include client protocol/token/endpoint.
[[nodiscard]] ConnectionFailure connection_failure(ConnectionRoutingError error, Command command) noexcept;
} // namespace dc24h::adc
