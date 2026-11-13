#include "dc24h/transfers/token.hpp"
#include "dc24h/text/public_text.hpp"

namespace dc24h::transfers {
std::optional<TokenError> validate_token(std::string_view value) {
    if (value.empty()) { return TokenError::empty; }
    if (value.size() > max_token_bytes) { return TokenError::too_long; }
    if (!text::valid_public_text(value)) { return TokenError::invalid_text; }
    return {};
}
std::variant<Token, TokenError> Token::create(std::string_view value) {
    if (const auto error = validate_token(value)) { return *error; }
    return Token(value);
}
} // namespace dc24h::transfers
