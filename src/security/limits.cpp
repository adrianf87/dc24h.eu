#include "dc24h/security/limits.hpp"

#include <stdexcept>
#include <utility>

namespace dc24h::security {
namespace {
constexpr auto minute = std::chrono::minutes(1);
}

void validate(const Limits& limits) {
    if (limits.max_connections_per_ip == 0 || limits.max_connections_per_ip > 65535 ||
        limits.max_accepts_per_minute == 0 || limits.max_accepts_per_minute > 100000 ||
        limits.max_frames_per_second == 0 || limits.max_frames_per_second > 100000 ||
        limits.max_invalid_frames == 0 || limits.max_invalid_frames > 65535 ||
        limits.tls_handshake_timeout < std::chrono::seconds(1) ||
        limits.tls_handshake_timeout > std::chrono::hours(1) ||
        limits.partial_frame_timeout < std::chrono::seconds(1) ||
        limits.partial_frame_timeout > std::chrono::hours(1) ||
        limits.login_timeout < std::chrono::seconds(1) || limits.login_timeout > std::chrono::hours(1) ||
        limits.output_timeout < std::chrono::seconds(1) || limits.output_timeout > std::chrono::hours(1) ||
        limits.close_timeout < std::chrono::seconds(1) || limits.close_timeout > std::chrono::hours(1) ||
        limits.max_pending_logins == 0 || limits.max_pending_logins > 65535 ||
        limits.max_pending_per_ip == 0 || limits.max_pending_per_ip > 65535 ||
        limits.max_login_bytes == 0 || limits.max_login_bytes > 16 * 1024 * 1024 ||
        limits.max_login_frames == 0 || limits.max_login_frames > 65535 ||
        limits.max_identity_checks_per_minute == 0 || limits.max_identity_checks_per_minute > 100000 ||
        limits.max_identity_checks_per_second == 0 || limits.max_identity_checks_per_second > 100000) {
        throw std::invalid_argument("Invalid security limits");
    }
}

PeerLimiter::PeerLimiter(Limits limits) : limits_(std::move(limits)) {
    validate(limits_);
}

void PeerLimiter::prune(Clock::time_point now) {
    if (last_prune_ && now - *last_prune_ < std::chrono::seconds(5)) { return; }
    last_prune_ = now;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.active == 0 && now - it->second.window_started >= minute &&
            (!it->second.identity_started || now - it->second.identity_window >= minute)) {
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

bool PeerLimiter::admit(std::string_view peer, Clock::time_point now) {
    prune(now);
    if (peer.empty() || peer.size() > max_peer_key_bytes || pending_ >= limits_.max_pending_logins) { return false; }
    // No unbounded per-source address cache under a many-IP connection flood.
    auto found = entries_.find(std::string(peer));
    if (found == entries_.end()) {
        if (entries_.size() >= max_tracked_peers) { return false; }
        Entry fresh{}; fresh.window_started = now;
        found = entries_.emplace(std::string(peer), fresh).first;
    }
    Entry& entry = found->second;
    if (now - entry.window_started >= minute) {
        entry.window_started = now;
        entry.attempts = 0;
    }
    if (entry.active >= limits_.max_connections_per_ip ||
        entry.attempts >= limits_.max_accepts_per_minute || entry.pending >= limits_.max_pending_per_ip) {
        return false;
    }
    ++entry.active;
    ++entry.attempts;
    ++entry.pending;
    ++pending_;
    return true;
}

void PeerLimiter::release(const std::string& peer, bool negotiating) noexcept {
    const auto found = entries_.find(peer);
    if (found != entries_.end() && found->second.active != 0) {
        --found->second.active;
        if (negotiating && found->second.pending != 0) { --found->second.pending; --pending_; }
    }
}
void PeerLimiter::complete_login(const std::string& peer) noexcept {
    const auto found = entries_.find(peer);
    if (found != entries_.end() && found->second.pending != 0) { --found->second.pending; --pending_; }
}
bool PeerLimiter::identity_check(const std::string& peer, Clock::time_point now) noexcept {
    const auto found = entries_.find(peer);
    if (found == entries_.end() || found->second.active == 0 || found->second.pending == 0) { return false; }
    auto& entry = found->second;
    const bool reset = !entry.identity_started || now - entry.identity_window >= minute;
    if (!reset && entry.identity_checks >= limits_.max_identity_checks_per_minute) { return false; }
    if (!identity_window_ || now - *identity_window_ >= std::chrono::seconds(1)) {
        identity_window_ = now; identity_checks_ = 0;
    }
    if (identity_checks_ >= limits_.max_identity_checks_per_second) { return false; }
    if (reset) { entry.identity_started = true; entry.identity_window = now; entry.identity_checks = 0; }
    ++identity_checks_;
    ++entry.identity_checks;
    return true;
}

bool FrameLimiter::admit(Clock::time_point now) noexcept {
    if (!started_ || now - window_started_ >= std::chrono::seconds(1)) {
        started_ = true;
        window_started_ = now;
        count_ = 0;
    }
    if (count_ >= maximum_) { return false; }
    ++count_;
    return true;
}
bool NegotiationBudget::admit_bytes(std::size_t bytes) noexcept {
    if (failed_ || bytes > max_bytes_ - bytes_) { failed_ = true; return false; }
    bytes_ += bytes;
    return true;
}
bool NegotiationBudget::admit_frame() noexcept {
    if (failed_ || frames_ >= max_frames_) { failed_ = true; return false; }
    ++frames_;
    return true;
}

} // namespace dc24h::security
