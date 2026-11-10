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
    std::size_t max_pending_logins{256};
    std::size_t max_pending_per_ip{64};
    std::size_t max_login_bytes{65536};
    std::uint32_t max_login_frames{64};
    std::uint32_t max_identity_checks_per_minute{64};
    std::uint32_t max_identity_checks_per_second{128};
};

// Reject zero and unreasonably large values. The caller must also verify
// global limits such as Network::max_connections.
void validate(const Limits& limits);

// One instance per network event loop. Caller supplies canonical, bounded IP keys.
// Successful admissions (not rejected attempts) count against the minute budget.
// IPs are stored only in memory and
// are never written to logs. Complete pending ownership at actual readiness;
// release exactly once with that connection's negotiating flag on cleanup.
class PeerLimiter final {
public:
    explicit PeerLimiter(Limits limits);
    [[nodiscard]] bool admit(std::string_view peer, Clock::time_point now);
    void release(const std::string& peer, bool negotiating = true) noexcept;
    // Exactly once at completed own-INF delivery, not merely valid identity.
    void complete_login(const std::string& peer) noexcept;
    // Shared per-IP minute and hub second windows. Charge before an initial
    // INF may reach the verifier; rejected checks never allocate map entries.
    [[nodiscard]] bool identity_check(const std::string& peer, Clock::time_point now) noexcept;
    [[nodiscard]] std::size_t tracked_peers() const noexcept { return entries_.size(); }
    [[nodiscard]] std::size_t pending_logins() const noexcept { return pending_; }

private:
    struct Entry {
        std::size_t active{0};
        std::uint32_t attempts{0};
        Clock::time_point window_started{};
        std::size_t pending{0};
        std::uint32_t identity_checks{0};
        Clock::time_point identity_window{};
        bool identity_started{false};
    };
    void prune(Clock::time_point now);
    Limits limits_;
    std::unordered_map<std::string, Entry> entries_;
    std::optional<Clock::time_point> last_prune_;
    std::size_t pending_{0};
    std::uint32_t identity_checks_{0};
    std::optional<Clock::time_point> identity_window_;
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

// Per-connection cumulative plaintext input through complete login, including
// LF, invalid/discarded bytes and keepalives. No window/reset/clock: a failed
// negotiation stays failed. The owner stops charging after readiness/closure.
class NegotiationBudget final {
public:
    NegotiationBudget(std::size_t bytes, std::uint32_t frames) noexcept : max_bytes_(bytes), max_frames_(frames) {}
    [[nodiscard]] bool admit_bytes(std::size_t bytes) noexcept;
    [[nodiscard]] bool admit_frame() noexcept;
    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] std::size_t bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::uint32_t frames() const noexcept { return frames_; }
private:
    std::size_t max_bytes_;
    std::uint32_t max_frames_;
    std::size_t bytes_{0};
    std::uint32_t frames_{0};
    bool failed_{false};
};

} // namespace dc24h::security
