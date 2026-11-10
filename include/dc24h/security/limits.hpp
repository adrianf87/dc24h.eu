#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dc24h::security {

// Local resource limits, not an ADC wire-protocol specification.
using Clock = std::chrono::steady_clock;
inline constexpr std::size_t max_tracked_peers = 65536;
inline constexpr std::size_t max_peer_key_bytes = 48;

struct Limits {
    std::size_t max_connections_per_ip{128};
    std::uint32_t max_accepts_per_minute{256};
    std::uint32_t max_frames_per_second{1024};
    std::uint32_t max_invalid_frames{512};
    std::chrono::seconds tls_handshake_timeout{10};
    std::chrono::seconds partial_frame_timeout{10};
    std::chrono::seconds login_timeout{30};
    std::chrono::seconds output_timeout{10};
    std::chrono::seconds close_timeout{2};
};

// Reject zero and unreasonably large values. The caller must also verify
// global limits such as Network::max_connections.
void validate(const Limits& limits);

// One instance per network event loop. Caller supplies canonical, bounded IP keys.
// Successful admissions (not rejected attempts) count against the minute budget.
// IPs are stored only in memory and
// are never written to logs. Call release() exactly once per accepted peer.
class PeerLimiter final {
public:
    explicit PeerLimiter(Limits limits);
    [[nodiscard]] bool admit(std::string_view peer, Clock::time_point now);
    void release(const std::string& peer) noexcept;
    [[nodiscard]] std::size_t tracked_peers() const noexcept { return entries_.size(); }

private:
    struct Entry {
        std::size_t active{0};
        std::uint32_t attempts{0};
        Clock::time_point window_started{};
    };
    void prune(Clock::time_point now);
    Limits limits_;
    std::unordered_map<std::string, Entry> entries_;
    std::optional<Clock::time_point> last_prune_;
};

// Simple fixed one-second window: one instance per connection. Every ADC
// frame (including invalid and keepalive) consumes a unit of the budget.
class FrameLimiter final {
public:
    explicit FrameLimiter(std::uint32_t max_per_second) noexcept
        : maximum_(max_per_second) {}
    [[nodiscard]] bool admit(Clock::time_point now) noexcept;
private:
    std::uint32_t maximum_;
    std::uint32_t count_{0};
    Clock::time_point window_started_{};
    bool started_{false};
};

} // namespace dc24h::security
