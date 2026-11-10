#include "dc24h/adc/state.hpp"
#include "dc24h/adc/sid.hpp"
#include "dc24h/adc/status.hpp"

#include <algorithm>
#include <initializer_list>

namespace dc24h::adc {
namespace {
bool is(MessageType type, std::initializer_list<MessageType> types) noexcept {
    return std::find(types.begin(), types.end(), type) != types.end();
}

// Deliberately bounded BASE hub profile, not a generic extension dispatcher.
bool valid_type(Direction direction, const Message& message) noexcept {
    using enum MessageType;
    const bool incoming = direction == Direction::from_client;
    switch (message.kind) {
    case Command::sup: case Command::pas:
        return message.type == (incoming ? hub : info);
    case Command::sid: case Command::gpa: case Command::qui:
        return !incoming && message.type == info;
    case Command::inf:
        return incoming ? message.type == broadcast : is(message.type, {info, broadcast});
    case Command::sta: case Command::msg:
        return is(message.type, {direct, echo, broadcast, feature}) ||
               message.type == (incoming ? hub : info);
    case Command::sch: case Command::res: case Command::ctm: case Command::rcm:
        return is(message.type, {broadcast, feature, direct, echo});
    default: return false;
    }
}

bool valid_state(ConnectionState state, Command command) noexcept {
    if (command == Command::sta) { return state != ConnectionState::disconnected; }
    switch (state) {
    case ConnectionState::protocol: return command == Command::sup || command == Command::sid;
    case ConnectionState::identify: return command == Command::inf || command == Command::qui;
    case ConnectionState::verify:
        return command == Command::gpa || command == Command::pas || command == Command::qui;
    case ConnectionState::normal:
        return command == Command::sup || command == Command::inf || command == Command::msg ||
               command == Command::sch || command == Command::res || command == Command::ctm ||
               command == Command::rcm || command == Command::qui;
    case ConnectionState::disconnected: return false;
    }
    return false;
}

bool valid_sid(std::string_view value) noexcept {
    return Sid::parse(value).has_value();
}
} // namespace

StateResult ConnectionStateMachine::observe(Direction direction, const Message& message) {
    if (state_ == ConnectionState::disconnected) { return StateError::disconnected; }
    // Defense against inconsistent hand-built Message objects. Argument/text
    // syntax remains the parser's responsibility, not a second parser here.
    if (recognize_command(message.command) != message.kind) { return StateError::invalid_message; }
    switch (message.kind) {
    case Command::unknown: case Command::get: case Command::gfi: case Command::snd:
        return StateError::unsupported_command;
    default: break;
    }
    if (!valid_type(direction, message)) { return StateError::invalid_type; }
    if (!valid_state(state_, message.kind)) { return StateError::invalid_state; }
    const bool incoming = direction == Direction::from_client;
    const bool routed = is(message.type, {MessageType::broadcast, MessageType::direct,
                                         MessageType::echo, MessageType::feature});
    if (incoming && routed && (sid_.empty() || message.source != sid_)) {
        return StateError::wrong_source;
    }
    if (message.kind == Command::pas && !incoming) { return StateError::invalid_type; }

    // During login only HSTA/ISTA is meaningful; routed status requires NORMAL.
    if (message.kind == Command::sta) {
        if (state_ != ConnectionState::normal && routed) { return StateError::invalid_type; }
        const auto status = validate_status(message);
        if (!std::holds_alternative<StatusView>(status)) {
            return StateError::invalid_message;
        }
        // A fatal hub-peer status terminates this connection; a forwarded fatal
        // status belongs to its recipient and must not terminate the observer.
        if (!routed && std::get<StatusView>(status).code.severity() == Severity::fatal) { disconnect(); }
        return std::nullopt;
    }
    if (state_ == ConnectionState::protocol) {
        if (message.kind == Command::sup) {
            if (incoming) {
                if (client_sup_) { return StateError::invalid_sequence; }
                client_sup_ = true;
            } else {
                if (!protocol_approved_ || hub_sup_) { return StateError::invalid_sequence; }
                hub_sup_ = true;
            }
        } else { // ISID; move to IDENTIFY only after the ordered handshake reply.
            if (!hub_sup_) { return StateError::invalid_sequence; }
            if (message.parameters.empty() || !valid_sid(message.parameters[0])) {
                return StateError::invalid_message;
            }
            sid_ = message.parameters[0]; // Allocate before mutating the state.
            state_ = ConnectionState::identify;
        }
    } else if (message.kind == Command::qui) {
        if (!std::holds_alternative<QuitView>(validate_quit(message))) {
            return StateError::invalid_message;
        }
        if (message.parameters[0] == sid_) { disconnect(); }
    } else if (state_ == ConnectionState::identify && message.kind == Command::inf) {
        if (incoming) {
            if (identity_pending_) { return StateError::invalid_sequence; }
            identity_pending_ = true;
        } else if (message.type != MessageType::info) {
            // Only the optional hub IINF is sent before identity approval.
            return StateError::invalid_sequence;
        }
    } else if (state_ == ConnectionState::verify) {
        if (message.kind == Command::gpa) {
            if (challenge_sent_) { return StateError::invalid_sequence; }
            challenge_sent_ = true;
        } else { // HPAS
            if (!challenge_sent_ || password_pending_) { return StateError::invalid_sequence; }
            password_pending_ = true;
        }
    }
    return std::nullopt;
}

StateResult ConnectionStateMachine::keepalive() const noexcept {
    if (state_ == ConnectionState::disconnected) { return StateError::disconnected; }
    return std::nullopt;
}

StateResult ConnectionStateMachine::approve_protocol() noexcept {
    if (state_ == ConnectionState::disconnected) { return StateError::disconnected; }
    if (state_ != ConnectionState::protocol) { return StateError::invalid_state; }
    if (!client_sup_ || protocol_approved_) { return StateError::invalid_sequence; }
    protocol_approved_ = true;
    return std::nullopt;
}

StateResult ConnectionStateMachine::approve_identity(IdentityMode mode) noexcept {
    if (state_ == ConnectionState::disconnected) { return StateError::disconnected; }
    if (state_ != ConnectionState::identify) { return StateError::invalid_state; }
    if (!identity_pending_) { return StateError::invalid_sequence; }
    identity_pending_ = false;
    state_ = mode == IdentityMode::guest ? ConnectionState::normal : ConnectionState::verify;
    return std::nullopt;
}

StateResult ConnectionStateMachine::approve_password() noexcept {
    if (state_ == ConnectionState::disconnected) { return StateError::disconnected; }
    if (state_ != ConnectionState::verify) { return StateError::invalid_state; }
    if (!password_pending_) { return StateError::invalid_sequence; }
    password_pending_ = false;
    state_ = ConnectionState::normal;
    return std::nullopt;
}

void ConnectionStateMachine::disconnect() noexcept {
    state_ = ConnectionState::disconnected;
    client_sup_ = protocol_approved_ = hub_sup_ = identity_pending_ = false;
    challenge_sent_ = password_pending_ = false;
}

} // namespace dc24h::adc
