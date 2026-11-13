#include "dc24h/adc/ctm.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/sid.hpp"
#include "dc24h/transfers/request.hpp"
#include "dc24h/text/public_text.hpp"
#include <algorithm>

namespace dc24h::adc {
namespace {
std::optional<CtmError> preflight(const Message& message) noexcept {
    if (message.kind != Command::ctm || message.command != "CTM" ||
        (message.positional_count && *message.positional_count != 3)) { return CtmError::invalid_message; }
    if (message.type != MessageType::direct && message.type != MessageType::echo) { return CtmError::invalid_type; }
    if (!message.source || !Sid::parse(*message.source) || !message.target || !Sid::parse(*message.target) || !message.features.empty()) {
        return CtmError::invalid_message;
    }
    if (message.parameters.size() < 3) { return CtmError::invalid_message; }
    if (message.parameters.size() > max_parameters) { return CtmError::field_limit; }
    std::size_t bytes = 0;
    for (const auto& value : message.parameters) {
        if (value.size() > max_ctm_bytes - bytes) { return CtmError::field_limit; }
        bytes += value.size();
    }
    return {};
}
std::variant<std::uint16_t, CtmError> fields(const Message& message) {
    const auto& p = message.parameters;
    if (p[0].empty() || p[0].size() > transfers::max_protocol_bytes ||
        !std::all_of(p[0].begin(), p[0].end(), [](unsigned char c) { return c >= 33 && c <= 126; })) {
        return CtmError::invalid_protocol;
    }
    const auto port = parse_integer(p[1]);
    if (!port || *port < 1 || *port > 65535) { return CtmError::invalid_port; }
    if (p[2].empty() || p[2].size() > transfers::max_token_bytes || !text::valid_public_text(p[2])) { return CtmError::invalid_token; }
    return static_cast<std::uint16_t>(*port);
}
} // namespace
CtmParseResult parse_ctm(std::string_view body) {
    auto parsed = parse_line(body);
    if (const auto* error = std::get_if<ParseError>(&parsed)) { return *error; }
    auto* message = std::get_if<Message>(&parsed);
    if (!message) { return CtmError::invalid_message; }
    if (const auto error = preflight(*message)) { return *error; }
    const auto checked = fields(*message);
    if (const auto* error = std::get_if<CtmError>(&checked)) { return *error; }
    return CtmRequest(std::move(*message), std::get<std::uint16_t>(checked));
}
std::variant<CtmRequest, CtmError> validate_ctm(const Message& message) {
    if (const auto error = preflight(message)) { return *error; }
    // Also reject inconsistent hand-built objects, malformed extensions and text.
    if (!std::holds_alternative<std::string>(serialize(message))) { return CtmError::invalid_message; }
    const auto checked = fields(message);
    if (const auto* error = std::get_if<CtmError>(&checked)) { return *error; }
    return CtmRequest(message, std::get<std::uint16_t>(checked));
}
} // namespace dc24h::adc
