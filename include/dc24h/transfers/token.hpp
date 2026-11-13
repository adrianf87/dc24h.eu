#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace dc24h::transfers {
inline constexpr std::size_t max_token_bytes = 128;
enum class TokenError { empty, too_long, invalid_text };
// Decoded connection token policy, shared by Core, CTM/RCM and peer errors.
// Bounds precede text processing; normalization allocation failures propagate.
// Tokens are opaque, case-sensitive NFC UTF-8. Never trim or normalize them.
[[nodiscard]] std::optional<TokenError> validate_token(std::string_view value);
class Token final {
public:
    [[nodiscard]] static std::variant<Token, TokenError> create(std::string_view value);
    [[nodiscard]] const std::string& value() const noexcept { return value_; }
    friend bool operator==(const Token&, const Token&) = default;
private:
    explicit Token(std::string_view value) : value_(value) {}
    std::string value_;
};
// This is a value, not a registry, credential, owner key or replay guard.
// Independent requests may use identical bytes. Routing uses ready SID lives.
} // namespace dc24h::transfers
