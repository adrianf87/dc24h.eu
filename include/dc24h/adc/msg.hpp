#pragma once

#include "dc24h/adc/msg_flags.hpp"

namespace dc24h::adc {
struct MsgView {
    std::string_view text; // Borrowed decoded UTF-8/NFC; escaped LF is allowed.
    std::optional<Sid> private_group;
    std::optional<bool> third_person; // Distinguishes absent ME from explicit ME0.
    [[nodiscard]] MsgFlags flags() const noexcept { return {private_group, third_person}; }
};
using MsgParseResult = std::variant<Message, ParseError, MsgError>;
using MsgWireResult = std::variant<std::string, MsgError>;

// One body WITHOUT LF; syntax errors retain the generic parser's error type.
// B/D/E/F/H/I only. One text parameter, optional PM<sid> and decimal ME with value 0 or 1.
// Unknown named parameters retain their order/values for extension-aware callers.
[[nodiscard]] MsgParseResult parse_msg(std::string_view body);
// Checks hostile hand-built objects too, including encoded frame and text bounds.
// Returned text view borrows Message storage until mutation/destruction.
[[nodiscard]] std::variant<MsgView, MsgError> validate_msg(const Message& message);
// Produces one owning, LF-terminated frame using the shared ADC escaping rules.
[[nodiscard]] MsgWireResult serialize_msg(const Message& message);

struct MsgOptions {
    MessageType type{MessageType::info};
    std::optional<Sid> source, target, private_group;
    std::optional<bool> third_person;
    std::vector<FeatureSelector> features;
};
// Checked local builder. Throws invalid_argument rather than truncating or
// normalizing invalid text/options. Output contains only the specified BASE flags.
[[nodiscard]] Message make_msg(std::string_view text, MsgOptions options = {});
// Codec validation grants no permission to send, route or impersonate a SID.
// PM may name a group different from the source; conversation policy is separate.
} // namespace dc24h::adc
