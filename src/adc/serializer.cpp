#include "dc24h/adc/serializer.hpp"

namespace dc24h::adc {
std::variant<std::string, SerializeError> serialize(const Message& message) {
    if (message.command.size() != 3 || recognize_command(message.command) != message.kind ||
        message.parameters.size() > max_parameters || message.features.size() > max_feature_selectors) {
        return SerializeError::invalid_message;
    }
    const auto type = message.type;
    const bool source = type == MessageType::broadcast || type == MessageType::direct ||
        type == MessageType::echo || type == MessageType::feature || type == MessageType::udp;
    const bool target = type == MessageType::direct || type == MessageType::echo;
    if (source != message.source.has_value() || target != message.target.has_value() ||
        (type == MessageType::feature ? message.features.empty() : !message.features.empty())) {
        return SerializeError::invalid_message;
    }
    std::string line(1, static_cast<char>(type));
    line += message.command;
    // Every append is bounded, including hostile hand-built Message objects.
    auto raw = [&](std::string_view value) {
        if (value.size() > max_frame_bytes - line.size()) { return false; }
        line += value;
        return true;
    };
    if (message.source && (!raw(" ") || !raw(*message.source))) { return SerializeError::frame_too_long; }
    if (message.target && (!raw(" ") || !raw(*message.target))) { return SerializeError::frame_too_long; }
    if (!message.features.empty()) {
        if (!raw(" ")) { return SerializeError::frame_too_long; }
        for (const auto& feature : message.features) {
            if (!raw(feature.required ? "+" : "-") || !raw(feature.name)) { return SerializeError::frame_too_long; }
        }
    }
    for (const auto& parameter : message.parameters) {
        if (parameter.empty()) { return SerializeError::invalid_message; }
        if (!raw(" ")) { return SerializeError::frame_too_long; }
        for (const char c : parameter) {
            const char single[]{c};
            const std::string_view encoded = c == ' ' ? "\\s" : c == '\n' ? "\\n" :
                c == '\\' ? "\\\\" : std::string_view(single, 1);
            if (!raw(encoded)) { return SerializeError::frame_too_long; }
        }
    }
    const auto parsed = parse_line(line);
    const auto* checked = std::get_if<Message>(&parsed);
    if (!checked || checked->type != type || checked->source != message.source ||
        checked->command != message.command || checked->target != message.target ||
        checked->parameters != message.parameters || checked->features.size() != message.features.size() ||
        (message.positional_count && checked->positional_count != message.positional_count)) {
        return SerializeError::invalid_message;
    }
    for (std::size_t i = 0; i < message.features.size(); ++i) {
        if (checked->features[i].required != message.features[i].required ||
            checked->features[i].name != message.features[i].name) { return SerializeError::invalid_message; }
    }
    line += '\n';
    return line;
}
} // namespace dc24h::adc
