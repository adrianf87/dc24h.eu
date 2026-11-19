#pragma once

#include <cstddef>
#include <cstdint>

namespace dc24h::users {
struct SessionCounts {
    std::size_t total{}, accepted{}, negotiating{}, identified{}, ready{}, closing{}, closed{};
    friend bool operator==(const SessionCounts&, const SessionCounts&) = default;
};
// Owned by SessionManager on its reactor thread. Values contain no user data.
// Online means ready now, after the final welcome LF, until closing begins.
struct Statistics {
    SessionCounts sessions{};
    std::size_t peak_online{};
    std::uint64_t created_sessions{}, successful_logins{}, completed_sessions{};
    // Accept-to-ready samples. A repeated ready transition is not a new sample.
    std::uint64_t login_samples{}, login_last_ms{}, login_max_ms{};
    [[nodiscard]] std::size_t online() const noexcept { return sessions.ready; }
    friend bool operator==(const Statistics&, const Statistics&) = default;
};
} // namespace dc24h::users
