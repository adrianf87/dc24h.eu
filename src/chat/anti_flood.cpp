#include "dc24h/chat/anti_flood.hpp"

#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace dc24h::chat {
namespace {
constexpr std::uint64_t scale = 1000000000;
// Saturate at a horizon sufficient to refill the largest legal bucket at its
// slowest rate. Unsigned tick subtraction avoids signed overflow even for
// synthetic min/max time points. Conversion happens only below that horizon.
std::uint64_t elapsed(FloodClock::time_point from, FloodClock::time_point to) noexcept {
    if (to <= from) { return 0; }
    using Rep = FloodClock::duration::rep;
    using Unsigned = std::make_unsigned_t<Rep>;
    const auto ticks = static_cast<Unsigned>(to.time_since_epoch().count()) - static_cast<Unsigned>(from.time_since_epoch().count());
    const auto horizon = std::chrono::duration_cast<FloodClock::duration>(std::chrono::seconds(max_byte_rate));
    if (ticks >= static_cast<Unsigned>(horizon.count())) { return static_cast<std::uint64_t>(max_byte_rate) * scale; }
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(FloodClock::duration(static_cast<Rep>(ticks))).count());
}
void refill(std::uint64_t& credit, std::uint32_t capacity, std::uint32_t rate, std::uint64_t nanoseconds) noexcept {
    const auto missing = static_cast<std::uint64_t>(capacity) * scale - credit;
    if (nanoseconds >= (missing + rate - 1) / rate) { credit += missing; }
    else { credit += nanoseconds * rate; } // Product is strictly below missing.
}
}
void validate(const FloodLimits& limits) {
    if (limits.messages_per_second == 0 || limits.messages_per_second > max_message_rate ||
        limits.message_burst == 0 || limits.message_burst > max_message_rate ||
        limits.bytes_per_second == 0 || limits.bytes_per_second > max_byte_rate ||
        limits.byte_burst < 16384 || limits.byte_burst > max_byte_rate) {
        throw std::invalid_argument("Invalid chat anti-flood limits");
    }
}
FloodGuard::FloodGuard(FloodLimits limits) : limits_(limits),
    messages_(static_cast<std::uint64_t>(limits.message_burst) * scale),
    bytes_(static_cast<std::uint64_t>(limits.byte_burst) * scale) { validate(limits_); }

FloodDecision FloodGuard::admit(std::size_t decoded_bytes, FloodClock::time_point now) noexcept {
    if (updated_) {
        now = std::max(now, *updated_); // A backwards test clock cannot mint credit.
        const auto delta = elapsed(*updated_, now);
        refill(messages_, limits_.message_burst, limits_.messages_per_second, delta);
        refill(bytes_, limits_.byte_burst, limits_.bytes_per_second, delta);
    }
    updated_ = now;
    if (decoded_bytes <= limits_.byte_burst && messages_ >= scale && bytes_ >= decoded_bytes * scale) {
        messages_ -= scale;
        bytes_ -= decoded_bytes * scale;
        return {true, false};
    }
    const bool notify = !notice_ || elapsed(*notice_, now) >= scale;
    if (notify) { notice_ = now; }
    return {false, notify};
}
} // namespace dc24h::chat
