#pragma once

#include "dc24h/adc/parser.hpp"
#include "dc24h/adc/sid.hpp"
#include "dc24h/transfers/request.hpp"
#include "dc24h/text/public_text.hpp"
#include <algorithm>

namespace dc24h::adc::detail {
// Shared D/E CTM/RCM wire boundaries, before normalization or owning copies.
template<class Error>
std::optional<Error> connection_preflight(const Message& message, Command kind, std::string_view command,
                                        std::size_t positional, std::size_t limit) noexcept {
    if (message.kind != kind || message.command != command ||
        (message.positional_count && *message.positional_count != positional)) { return Error::invalid_message; }
    if (message.type != MessageType::direct && message.type != MessageType::echo) { return Error::invalid_type; }
    if (!message.source || !Sid::parse(*message.source) || !message.target || !Sid::parse(*message.target) || !message.features.empty()) {
        return Error::invalid_message;
    }
    if (message.parameters.size() < positional) { return Error::invalid_message; }
    if (message.parameters.size() > max_parameters) { return Error::field_limit; }
    std::size_t bytes = 0;
    for (const auto& value : message.parameters) {
        if (value.size() > limit - bytes) { return Error::field_limit; }
        bytes += value.size();
    }
    return {};
}
inline bool connection_protocol(std::string_view value) noexcept {
    return !value.empty() && value.size() <= transfers::max_protocol_bytes &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 33 && c <= 126; });
}
inline bool connection_token(std::string_view value) {
    return !value.empty() && value.size() <= transfers::max_token_bytes && text::valid_public_text(value);
}
} // namespace dc24h::adc::detail
