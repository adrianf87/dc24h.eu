#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace dc24h::adc::detail {

inline constexpr std::size_t tiger_bytes = 24;

constexpr bool uppercase(char value) noexcept { return value >= 'A' && value <= 'Z'; }

constexpr bool digit(char value) noexcept { return value >= '0' && value <= '9'; }

constexpr bool alphanumeric(char value) noexcept { return uppercase(value) || digit(value); }

constexpr bool wire_name(std::string_view value, std::size_t width) noexcept {
    return !value.empty() && value.size() == width && uppercase(value.front()) &&
           std::ranges::all_of(value.substr(1), alphanumeric);
}

constexpr unsigned base32_value(char symbol) noexcept {
    if (uppercase(symbol)) {
        return static_cast<unsigned>(symbol - 'A');
    }
    if (symbol >= '2' && symbol <= '7') {
        return 26U + static_cast<unsigned>(symbol - '2');
    }
    return 32U;
}

constexpr bool base32_symbol(char symbol) noexcept { return base32_value(symbol) < 32U; }

// ADC encodes binary values without '=' padding. The unused low bits must be zero.
constexpr std::size_t base32_bytes(std::string_view value) noexcept {
    if (value.empty()) {
        return 0;
    }
    const auto tail_bits = (value.size() % 8U) * 5U % 8U;
    if (tail_bits >= 5U || !std::ranges::all_of(value, base32_symbol) ||
        (base32_value(value.back()) & ((1U << tail_bits) - 1U)) != 0U) {
        return 0;
    }
    return (value.size() / 8U) * 5U + (value.size() % 8U) * 5U / 8U;
}

} // namespace dc24h::adc::detail
