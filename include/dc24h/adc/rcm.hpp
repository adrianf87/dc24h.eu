#pragma once

#include "dc24h/adc/parser.hpp"

namespace dc24h::adc {
inline constexpr std::size_t max_rcm_bytes = 2048;
enum class RcmError { invalid_message, invalid_type, field_limit, invalid_protocol, invalid_token };
// Owns D/E RCM protocol/token and original extensions. RCM carries no port.
class RcmRequest final {
public:
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& protocol() const noexcept { return message_.parameters[0]; }
    [[nodiscard]] const std::string& token() const noexcept { return message_.parameters[1]; }
private:
    friend std::variant<RcmRequest, RcmError> validate_rcm(const Message& message);
    friend std::variant<RcmRequest, ParseError, RcmError> parse_rcm(std::string_view body);
    explicit RcmRequest(Message message) : message_(std::move(message)) {}
    Message message_;
};
using RcmParseResult = std::variant<RcmRequest, ParseError, RcmError>;
[[nodiscard]] RcmParseResult parse_rcm(std::string_view body);
[[nodiscard]] std::variant<RcmRequest, RcmError> validate_rcm(const Message& message);
} // namespace dc24h::adc
