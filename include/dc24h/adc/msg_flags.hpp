#pragma once

#include "dc24h/adc/parser.hpp"
#include "dc24h/adc/sid.hpp"

#include <span>

namespace dc24h::adc {
enum class MsgError {
    invalid_message, invalid_type, duplicate_field, invalid_private_group,
    invalid_third_person, frame_too_long
};

// Values, not routing authority. Absent ME and explicit ME0 are distinct;
// PM identifies the conversation's reply SID, not an additional recipient.
struct MsgFlags {
    std::optional<Sid> private_group;
    std::optional<bool> third_person;
    friend bool operator==(const MsgFlags&, const MsgFlags&) = default;
};

// Decoded named tokens ONLY, excluding MSG's positional text. Checks known
// flags and named-token shape without allocation. Unknown values are ignored;
// UTF-8/NFC, headers, total encoded size and authorization need the full codec
// and router. Duplicate PM/ME are errors, even with identical values or ME0.
[[nodiscard]] std::variant<MsgFlags, MsgError> parse_msg_flags(
    std::span<const std::string> fields) noexcept;

// Owning positional text followed by canonical PM, then ME0/ME1. Unknown fields
// are never copied. Throws invalid_argument on invalid chat text; allocation
// failures propagate without changing caller state. Full wire limits and
// header/scope checks remain with serialize_msg and MessageRouter.
[[nodiscard]] std::vector<std::string> make_msg_parameters(
    std::string_view text, const MsgFlags& flags);
} // namespace dc24h::adc
