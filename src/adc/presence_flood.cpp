#include "dc24h/adc/presence_flood.hpp"

#include <algorithm>
#include <type_traits>

namespace dc24h::adc {
namespace {
constexpr std::uint64_t scale = 1000000000;
// Eight seconds refills the burst more than once (burst / rate is 2s).
// Unsigned tick subtraction avoids signed overflow on synthetic clocks.
std::uint64_t elapsed(PresenceClock::time_point from, PresenceClock::time_point to) noexcept {
    if (to <= from) { return 0; }
    using Rep = PresenceClock::duration::rep;
    using Unsigned = std::make_unsigned_t<Rep>;
    const auto ticks = static_cast<Unsigned>(to.time_since_epoch().count()) - static_cast<Unsigned>(from.time_since_epoch().count());
    const auto horizon = std::chrono::duration_cast<PresenceClock::duration>(std::chrono::seconds(presence_update_burst));
    if (ticks >= static_cast<Unsigned>(horizon.count())) { return static_cast<std::uint64_t>(presence_update_burst) * scale; }
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(PresenceClock::duration(static_cast<Rep>(ticks))).count());
}
void refill(std::uint64_t& credit, std::uint64_t nanoseconds) noexcept {
    constexpr auto capacity = static_cast<std::uint64_t>(presence_update_burst);
    constexpr auto rate = static_cast<std::uint64_t>(presence_updates_per_second);
    const auto missing = capacity * scale - credit;
    if (nanoseconds >= (missing + rate - 1) / rate) { credit += missing; }
    else { credit += nanoseconds * rate; } // Product stays below missing.
}
}
PresenceGuard::PresenceGuard() noexcept : updates_(static_cast<std::uint64_t>(presence_update_burst) * scale) {}

PresenceDecision PresenceGuard::admit(PresenceClock::time_point now) noexcept {
    if (updated_) {
        now = std::max(now, *updated_); // A backwards test clock cannot mint credit.
        refill(updates_, elapsed(*updated_, now));
    }
    updated_ = now;
    if (updates_ >= scale) {
        updates_ -= scale;
        return {true, false};
    }
    const bool notify = !notice_ || elapsed(*notice_, now) >= scale;
    if (notify) { notice_ = now; }
    return {false, notify};
}
} // namespace dc24h::adc
