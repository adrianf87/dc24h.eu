#include "dc24h/adc/rcm.hpp"
#include "dc24h/adc/serializer.hpp"
#include "connection_fields.hpp"

namespace dc24h::adc {
namespace {
std::optional<RcmError> fields(const Message& message) {
    if (!detail::connection_protocol(message.parameters[0])) { return RcmError::invalid_protocol; }
    if (!detail::connection_token(message.parameters[1])) { return RcmError::invalid_token; }
    return {};
}
} // namespace
RcmParseResult parse_rcm(std::string_view body) {
    auto parsed = parse_line(body);
    if (const auto* error = std::get_if<ParseError>(&parsed)) { return *error; }
    auto* message = std::get_if<Message>(&parsed);
    if (!message) { return RcmError::invalid_message; }
    if (const auto error = detail::connection_preflight<RcmError>(*message, Command::rcm, "RCM", 2, max_rcm_bytes)) { return *error; }
    if (const auto error = fields(*message)) { return *error; }
    return RcmRequest(std::move(*message));
}
std::variant<RcmRequest, RcmError> validate_rcm(const Message& message) {
    if (const auto error = detail::connection_preflight<RcmError>(message, Command::rcm, "RCM", 2, max_rcm_bytes)) { return *error; }
    if (!std::holds_alternative<std::string>(serialize(message))) { return RcmError::invalid_message; }
    if (const auto error = fields(message)) { return *error; }
    return RcmRequest(message);
}
} // namespace dc24h::adc
