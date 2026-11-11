#include "dc24h/adc/msg_flags.hpp"
#include "dc24h/text/public_text.hpp"

#include <stdexcept>

namespace dc24h::adc {
std::variant<MsgFlags, MsgError> parse_msg_flags(std::span<const std::string> fields) noexcept {
    if (fields.size() >= max_parameters) { return MsgError::invalid_message; }
    MsgFlags result;
    for (const auto& token : fields) {
        if (token.size() > max_frame_bytes) { return MsgError::frame_too_long; }
        const auto field = parse_named_parameter(token);
        if (!field) { return MsgError::invalid_message; }
        if (field->name == "PM") {
            if (result.private_group) { return MsgError::duplicate_field; }
            result.private_group = Sid::parse(field->value);
            if (!result.private_group) { return MsgError::invalid_private_group; }
        } else if (field->name == "ME") {
            if (result.third_person.has_value()) { return MsgError::duplicate_field; }
            const auto value = parse_integer(field->value);
            if (!value || (*value != 0 && *value != 1)) { return MsgError::invalid_third_person; }
            result.third_person = *value == 1;
        }
    }
    return result;
}

std::vector<std::string> make_msg_parameters(std::string_view text, const MsgFlags& flags) {
    if (!text::valid_chat_text(text)) { throw std::invalid_argument("Invalid local MSG text"); }
    std::vector<std::string> parameters;
    parameters.reserve(1U + (flags.private_group ? 1U : 0U) + (flags.third_person.has_value() ? 1U : 0U));
    parameters.emplace_back(text);
    if (flags.private_group) { parameters.emplace_back("PM" + flags.private_group->str()); }
    if (flags.third_person.has_value()) { parameters.emplace_back(*flags.third_person ? "ME1" : "ME0"); }
    return parameters;
}
} // namespace dc24h::adc
