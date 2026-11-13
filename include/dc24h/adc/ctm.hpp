#pragma once

#include "dc24h/adc/parser.hpp"

namespace dc24h::adc {
inline constexpr std::size_t max_ctm_bytes = 2048;
enum class CtmError { invalid_message, invalid_type, field_limit, invalid_protocol, invalid_port, invalid_token };
// D/E CTM only. Owns original decoded fields/order and numeric spelling.
// Port is a peer listener declaration, never an instruction to open a hub socket.
class CtmRequest final {
public:
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& protocol() const noexcept { return message_.parameters[0]; }
    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
    [[nodiscard]] const std::string& token() const noexcept { return message_.parameters[2]; }
private:
    friend std::variant<CtmRequest, CtmError> validate_ctm(const Message& message);
    friend std::variant<CtmRequest, ParseError, CtmError> parse_ctm(std::string_view body);
    CtmRequest(Message message, std::uint16_t port) : message_(std::move(message)), port_(port) {}
    Message message_;
    std::uint16_t port_;
};
using CtmParseResult = std::variant<CtmRequest, ParseError, CtmError>;
[[nodiscard]] CtmParseResult parse_ctm(std::string_view body);
[[nodiscard]] std::variant<CtmRequest, CtmError> validate_ctm(const Message& message);
} // namespace dc24h::adc
