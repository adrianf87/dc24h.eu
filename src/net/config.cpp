#include "dc24h/net/config.hpp"

#include "dc24h/adc/parser.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace dc24h::net {
namespace {

std::size_t number(std::string_view value, std::size_t maximum, std::string_view name) {
    std::size_t parsed = 0;
    if (!value.empty()) {
        const auto end = value.data() + value.size();
        const auto result = std::from_chars(value.data(), end, parsed);
        if (result.ec == std::errc{} && result.ptr == end && parsed > 0 && parsed <= maximum) {
            return parsed;
        }
    }
    throw std::invalid_argument("Invalid numeric value for --" + std::string(name));
}

bool valid_text(std::string_view value, std::size_t limit) {
    return value.size() <= limit &&
           std::ranges::none_of(value,
                                [](unsigned char byte) { return byte < 32 || byte == 127; }) &&
           (value.empty() || std::holds_alternative<std::string>(
                                 adc::unescape_parameter(adc::escape_parameter(value))));
}

} // namespace

bool apply_hub_setting(ServerConfig &config, std::string_view name, std::string_view value) {
    if (name == "hub-name") {
        if (value.empty() || !valid_text(value, 128)) {
            throw std::invalid_argument("Invalid hub name");
        }
        config.hub_name = value;
    } else if (name == "hub-description") {
        if (!valid_text(value, 255)) {
            throw std::invalid_argument("Invalid hub description");
        }
        config.hub_description = value;
    } else if (name == "max-clients") {
        config.max_clients = number(value, 1048575, name);
    } else if (name == "max-nick-codepoints") {
        config.max_nick_codepoints = number(value, 256, name);
    } else if (name == "max-message-bytes") {
        config.max_message_bytes = number(value, 4096, name);
    } else if (name == "input-bytes") {
        config.limits.input_bytes = number(value, 1048576, name);
    } else if (name == "pending-bytes") {
        config.limits.pending_bytes = number(value, 1048576, name);
    } else {
        using Timeout = std::pair<std::string_view, std::chrono::milliseconds ConnectionLimits::*>;
        constexpr Timeout timeouts[] = {
            {"setup-timeout-ms", &ConnectionLimits::setup_timeout},
            {"tls-timeout-ms", &ConnectionLimits::tls_timeout},
            {"identify-timeout-ms", &ConnectionLimits::identify_timeout},
            {"idle-timeout-ms", &ConnectionLimits::idle_timeout},
            {"shutdown-timeout-ms", &ConnectionLimits::shutdown_timeout}};
        const auto matched = std::ranges::find(timeouts, name, &Timeout::first);
        if (matched == std::end(timeouts)) {
            return false;
        }
        using Rep = std::chrono::milliseconds::rep;
        config.limits.*(matched->second) =
            std::chrono::milliseconds(static_cast<Rep>(number(value, 86400000, name)));
    }
    return true;
}

} // namespace dc24h::net
