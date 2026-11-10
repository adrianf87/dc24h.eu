#include "dc24h/adc/msg.hpp"
#include "dc24h/adc/serializer.hpp"

#include <stdexcept>
#include <utility>

namespace dc24h::adc {
namespace {
std::variant<MsgView, MsgError> inspect(const Message& message) noexcept {
    if (message.command != "MSG" || message.kind != Command::msg ||
        message.parameters.empty() || message.parameters.size() > max_parameters ||
        message.parameters[0].empty() ||
        (message.positional_count && *message.positional_count != 1)) {
        return MsgError::invalid_message;
    }
    switch (message.type) {
    case MessageType::broadcast: case MessageType::direct: case MessageType::echo:
    case MessageType::feature: case MessageType::hub: case MessageType::info: break;
    default: return MsgError::invalid_type;
    }
    MsgView result{message.parameters[0], {}, {}};
    for (std::size_t i=1; i<message.parameters.size(); ++i) {
        const auto field=parse_named_parameter(message.parameters[i]);
        if (!field) { return MsgError::invalid_message; }
        if (field->name == "PM") {
            if (result.private_group) { return MsgError::duplicate_field; }
            result.private_group=Sid::parse(field->value);
            if (!result.private_group) { return MsgError::invalid_private_group; }
        } else if (field->name == "ME") {
            if (result.third_person.has_value()) { return MsgError::duplicate_field; }
            const auto value=parse_integer(field->value);
            if (!value || (*value != 0 && *value != 1)) { return MsgError::invalid_third_person; }
            result.third_person=*value == 1;
        }
    }
    return result;
}
MsgError wire_error(SerializeError error) noexcept {
    return error == SerializeError::frame_too_long ? MsgError::frame_too_long : MsgError::invalid_message;
}
} // namespace

MsgParseResult parse_msg(std::string_view body) {
    auto parsed=parse_line(body);
    if (const auto* error=std::get_if<ParseError>(&parsed)) { return *error; }
    auto* message=std::get_if<Message>(&parsed);
    if (!message) { return MsgError::invalid_message; }
    const auto result=inspect(*message); // parse_line already checked header/text/wire size.
    if (const auto* error=std::get_if<MsgError>(&result)) { return *error; }
    return std::move(*message);
}
std::variant<MsgView, MsgError> validate_msg(const Message& message) {
    const auto result=inspect(message);
    if (std::holds_alternative<MsgError>(result)) { return result; }
    const auto wire=serialize(message);
    if (const auto* error=std::get_if<SerializeError>(&wire)) { return wire_error(*error); }
    return result;
}
MsgWireResult serialize_msg(const Message& message) {
    const auto result=inspect(message);
    if (const auto* error=std::get_if<MsgError>(&result)) { return *error; }
    auto wire=serialize(message);
    if (const auto* error=std::get_if<SerializeError>(&wire)) { return wire_error(*error); }
    return std::move(std::get<std::string>(wire));
}
Message make_msg(std::string_view text, MsgOptions options) {
    if (text.empty() || text.size() > max_frame_bytes || options.features.size() > max_feature_selectors) {
        throw std::invalid_argument("Invalid local MSG options");
    }
    Message message{options.type, "MSG", Command::msg, {}, {}, std::move(options.features), {std::string(text)}, 1};
    if (options.source) { message.source=options.source->str(); }
    if (options.target) { message.target=options.target->str(); }
    if (options.private_group) { message.parameters.push_back("PM"+options.private_group->str()); }
    if (options.third_person.has_value()) { message.parameters.push_back(*options.third_person ? "ME1" : "ME0"); }
    if (!std::holds_alternative<std::string>(serialize_msg(message))) {
        throw std::invalid_argument("Invalid local MSG options");
    }
    return message;
}
} // namespace dc24h::adc
