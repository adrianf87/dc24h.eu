#pragma once

#include "dc24h/adc/state.hpp"

namespace dc24h::adc {

// Stateless hub command profile: direction, message type and command only.
// The parser still accepts unknown extensions and hub-legal frames. This
// function does not read parameters, features or text, and it does not allocate.
enum class ControlCode { ok, unsupported_command, invalid_type };

[[nodiscard]] ControlCode control_message(Direction direction, const Message& message) noexcept;

} // namespace dc24h::adc
