#pragma once

#include "dc24h/adc/parser.hpp"

namespace dc24h::adc {
enum class SerializeError { invalid_message, frame_too_long };
// Owning LF-terminated frame, at most max_frame_bytes+1 bytes. Does not grant
// permission to send. Empty parameters, inconsistent headers and invalid text
// fail; spaces, backslashes and newlines in decoded values are escaped.
[[nodiscard]] std::variant<std::string, SerializeError> serialize(const Message& message);
} // namespace dc24h::adc
