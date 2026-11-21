#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace dc24h::net {

struct Endpoint {
    std::string address;
    std::uint16_t port;
};

struct ConnectionLimits {
    std::size_t input_bytes = 16 * 1024;
    std::size_t pending_bytes = 64 * 1024;
    std::chrono::milliseconds setup_timeout{30000};
    std::chrono::milliseconds tls_timeout{10000};
    std::chrono::milliseconds identify_timeout{15000};
    std::chrono::milliseconds idle_timeout{300000};
    std::chrono::milliseconds shutdown_timeout{2000};
};

struct ServerConfig {
    Endpoint adcs{"127.0.0.1", 1511};
    std::optional<Endpoint> adc;
    std::filesystem::path certificate;
    std::filesystem::path private_key;
    std::size_t max_clients = 128;
    ConnectionLimits limits;
    std::size_t max_nick_codepoints = 32;
    std::size_t max_message_bytes = 1024;
    std::string hub_name;
    std::string hub_description;
};

// Return false for unknown names and throw invalid_argument for invalid values.
[[nodiscard]] bool apply_hub_setting(ServerConfig &config, std::string_view name,
                                     std::string_view value);

} // namespace dc24h::net
