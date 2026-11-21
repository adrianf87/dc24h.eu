#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>

namespace dc24h::hub {

class TokenBucket {
  public:
    using Clock = std::chrono::steady_clock;

    TokenBucket(std::size_t capacity, std::chrono::milliseconds interval, Clock::time_point now)
        : capacity_(capacity), interval_(interval), refill_(now), tokens_(capacity) {}

    [[nodiscard]] bool take(Clock::time_point now) {
        const auto elapsed = now - refill_;
        if (elapsed >= interval_) {
            const auto periods = elapsed / interval_;
            refill_ += periods * interval_;
            tokens_ += std::min(capacity_ - tokens_, static_cast<std::size_t>(periods));
        }
        if (tokens_ == 0) {
            return false;
        }
        --tokens_;
        return true;
    }

  private:
    std::size_t capacity_;
    std::chrono::milliseconds interval_;
    Clock::time_point refill_;
    std::size_t tokens_;
};

} // namespace dc24h::hub
