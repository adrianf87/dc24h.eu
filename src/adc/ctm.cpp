#include "dc24h/adc/ctm.hpp"
#include "dc24h/adc/serializer.hpp"
#include "connection_fields.hpp"

namespace dc24h::adc {
namespace {
std::optional<CtmError> preflight(const Message& message) noexcept {
    return detail::connection_preflight<CtmError>(message, Command::ctm, "CTM", 3, max_ctm_bytes);
}
std::variant<std::uint16_t, CtmError> fields(const Message& message) {
    const auto& p = message.parameters;
    if (!detail::connection_protocol(p[0])) { return CtmError::invalid_protocol; }
    const auto port = parse_integer(p[1]);
    if (!port || *port < 1 || *port > 65535) { return CtmError::invalid_port; }
    if (!detail::connection_token(p[2])) { return CtmError::invalid_token; }
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
