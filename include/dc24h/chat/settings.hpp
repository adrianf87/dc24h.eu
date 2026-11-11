#pragma once

#include "dc24h/chat/anti_flood.hpp"
#include "dc24h/text/public_text.hpp"

#include <string>
#include <string_view>

namespace dc24h::chat {
inline constexpr std::string_view default_welcome_message = "Welcome to dc24h.eu. Guest access is enabled.";
inline constexpr std::size_t max_welcome_bytes = 4096;
struct Policy {
    std::size_t max_text_bytes{text::max_chat_text_bytes};
    bool public_enabled{true};
    bool private_enabled{true};
    bool actions_enabled{true};
};
struct Settings {
    FloodLimits flood{};
    Policy policy{};
    bool welcome_enabled{true};
    std::string welcome_message{default_welcome_message};
};
// Startup-only checked values; invalid configuration throws before listeners.
void validate_policy(const Policy& policy);
void validate_welcome(std::string_view message, bool enabled = true);
void validate_settings(const Settings& settings);
} // namespace dc24h::chat
