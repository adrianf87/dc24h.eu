#include "dc24h/adc/chat_validation.hpp"
#include "dc24h/text/public_text.hpp"

namespace dc24h::adc {
ChatValidationResult validate_chat(const chat::Core& core, users::SessionId sender, const Message& message) {
    if (message.command != "MSG" || message.kind != Command::msg || message.parameters.empty() ||
        message.parameters.size() > max_parameters || (message.positional_count && *message.positional_count != 1)) {
        return ChatValidationError::invalid_message;
    }
    // Bound hostile hand-built objects before serializing or dereferencing SIDs.
    if (!text::valid_chat_text(message.parameters[0])) { return ChatValidationError::invalid_text; }
    const bool routed = message.type == MessageType::broadcast || message.type == MessageType::direct ||
        message.type == MessageType::echo || message.type == MessageType::feature;
    if (routed) {
        const auto source = message.source ? Sid::parse(*message.source) : std::nullopt;
        if (!source || source->value() == 0) { return ChatValidationError::invalid_sid; }
    }
    if ((message.type == MessageType::direct || message.type == MessageType::echo) &&
        (!message.target || !Sid::parse(*message.target))) { return ChatValidationError::invalid_sid; }
    const auto checked = validate_msg(message);
    const auto* view = std::get_if<MsgView>(&checked);
    if (!view) { return ChatValidationError::invalid_message; }
    if (!routed) { return ChatValidationError::unsupported_type; }
    if (const auto error = core.validate_sender(sender, *message.source)) {
        return *error == chat::Error::wrong_source ? ChatValidationError::wrong_source : ChatValidationError::invalid_state;
    }
    if (message.type == MessageType::feature) { return ChatValidationError::unsupported_type; }
    return *view;
}
} // namespace dc24h::adc
