#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace dc24h::chat {
using FloodClock = std::chrono::steady_clock;
inline constexpr std::uint32_t max_message_rate = 1000000;
inline constexpr std::uint32_t max_byte_rate = 16 * 1024 * 1024;
struct FloodLimits {
    std::uint32_t messages_per_second{5};
    std::uint32_t message_burst{10};
    std::uint32_t bytes_per_second{32768};
    std::uint32_t byte_burst{131072};
};
void validate(const FloodLimits& limits);
struct FloodDecision {
    bool allowed;
    bool notify;
    friend bool operator==(const FloodDecision&, const FloodDecision&) = default;
};
// One owner thread and one guard per connection lifetime. Both buckets start
// full and debit together; denial never partially consumes either budget.
// No message text/history/identity is retained. Notices are limited to one per
// second across B/D/E; a successful message does not reset that notice window.
class FloodGuard final {
public:
    explicit FloodGuard(FloodLimits limits = {});
    [[nodiscard]] FloodDecision admit(std::size_t decoded_bytes, FloodClock::time_point now) noexcept;
private:
    FloodLimits limits_;
    std::uint64_t messages_, bytes_; // Nano-tokens retain fractional refill exactly.
    std::optional<FloodClock::time_point> updated_, notice_;
};
} // namespace dc24h::chat
