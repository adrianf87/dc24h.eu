#include "dc24h/adc/protocol_control.hpp"

#include <algorithm>
#include <initializer_list>

namespace dc24h::adc {
namespace {
bool one_of(MessageType type, std::initializer_list<MessageType> types) noexcept {
    return std::find(types.begin(), types.end(), type) != types.end();
}
} // namespace

ControlCode control_message(Direction direction, const Message& message) noexcept {
    using enum MessageType;
    const bool incoming = direction == Direction::from_client;
    switch (message.kind) {
    case Command::unknown:
    case Command::get:
    case Command::gfi:
    case Command::snd:
        // Unsupported before the type check, including a hub-typed GET.
        return ControlCode::unsupported_command;
    case Command::pas:
        // Folded from the former late observe check: every outgoing PAS is an
        // invalid type. Only an incoming hub PAS matches the profile.
        return incoming && message.type == hub ? ControlCode::ok : ControlCode::invalid_type;
    case Command::sup:
        return message.type == (incoming ? hub : info) ? ControlCode::ok : ControlCode::invalid_type;
    case Command::sid:
    case Command::gpa:
    case Command::qui:
        return !incoming && message.type == info ? ControlCode::ok : ControlCode::invalid_type;
    case Command::inf:
        if (incoming) {
            return message.type == broadcast ? ControlCode::ok : ControlCode::invalid_type;
        }
        return one_of(message.type, {info, broadcast}) ? ControlCode::ok : ControlCode::invalid_type;
    case Command::sta:
    case Command::msg:
        if (one_of(message.type, {direct, echo, broadcast, feature}) ||
            message.type == (incoming ? hub : info)) {
            return ControlCode::ok;
        }
        return ControlCode::invalid_type;
    case Command::sch:
    case Command::res:
    case Command::ctm:
    case Command::rcm:
        return one_of(message.type, {broadcast, feature, direct, echo})
            ? ControlCode::ok : ControlCode::invalid_type;
    }
    return ControlCode::unsupported_command;
}

} // namespace dc24h::adc
