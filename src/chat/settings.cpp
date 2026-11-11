#include "dc24h/chat/settings.hpp"

#include <stdexcept>

namespace dc24h::chat {
void validate_policy(const Policy& policy) {
    if (policy.max_text_bytes == 0 || policy.max_text_bytes > text::max_chat_text_bytes) {
        throw std::invalid_argument("Invalid chat policy");
    }
}
void validate_welcome(std::string_view message, bool enabled) {
    if (message.empty() && !enabled) { return; }
    if (message.size() > max_welcome_bytes || !text::valid_chat_text(message)) {
        throw std::invalid_argument("Invalid chat welcome message");
    }
}
void validate_settings(const Settings& settings) {
    validate(settings.flood);
    validate_policy(settings.policy);
    validate_welcome(settings.welcome_message, settings.welcome_enabled);
}
} // namespace dc24h::chat
