#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace dc24h::adc {
using PresenceClock = std::chrono::steady_clock;
// Fixed per ready session. Not an INI key. Login INF is not charged.
inline constexpr std::uint32_t presence_updates_per_second = 4;
inline constexpr std::uint32_t presence_update_burst = 8;

struct PresenceDecision {
    bool allowed;
    bool notify;
    friend bool operator==(const PresenceDecision&, const PresenceDecision&) = default;
};

// One owner thread and one guard per connection lifetime. The bucket starts
// full. Denial does not consume credit. No field text is retained. Notices
// are limited to one per second. A successful update does not reset that window.
class PresenceGuard final {
public:
    PresenceGuard() noexcept;
    [[nodiscard]] PresenceDecision admit(PresenceClock::time_point now) noexcept;
private:
    std::uint64_t updates_;
    std::optional<PresenceClock::time_point> updated_, notice_;
};
} // namespace dc24h::adc
