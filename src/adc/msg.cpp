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
    const auto parsed = parse_msg_flags(std::span<const std::string>(message.parameters).subspan(1));
    if (const auto* error = std::get_if<MsgError>(&parsed)) { return *error; }
    const auto& flags = std::get<MsgFlags>(parsed);
    return MsgView{message.parameters[0], flags.private_group, flags.third_person};
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
    Message message{options.type, "MSG", Command::msg, {}, {}, std::move(options.features),
        make_msg_parameters(text, {options.private_group, options.third_person}), 1};
    if (options.source) { message.source=options.source->str(); }
    if (options.target) { message.target=options.target->str(); }
    if (!std::holds_alternative<std::string>(serialize_msg(message))) {
        throw std::invalid_argument("Invalid local MSG options");
    }
    return message;
}
} // namespace dc24h::adc
