#include "dc24h/stress/soak.hpp"

#include "dc24h/chat/anti_flood.hpp"
#include "dc24h/security/limits.hpp"
#include "dc24h/users/session_manager.hpp"

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace dc24h::stress {
namespace {

using Clock = std::chrono::steady_clock;

void demand(bool condition, const char* reason) {
    if (!condition) { throw std::runtime_error(reason); }
}

std::uint64_t milliseconds_since(Clock::time_point started) noexcept {
    const auto elapsed = Clock::now() - started;
    if (elapsed < Clock::duration::zero()) { return 0; }
    const auto count = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    if (count <= 0) { return 0; }
    return static_cast<std::uint64_t>(count);
}

security::Limits peer_limits() {
    security::Limits limits;
    limits.max_connections_per_ip = connections_per_ip;
    limits.max_accepts_per_minute = accepts_per_minute;
    limits.max_pending_logins = connections_per_ip;
    limits.max_pending_per_ip = connections_per_ip;
    limits.max_frames_per_second = frame_budget;
    limits.max_login_frames = login_frames;
    return limits;
}

chat::FloodLimits chat_limits() {
    chat::FloodLimits limits;
    limits.messages_per_second = 1;
    limits.message_burst = chat_burst;
    limits.bytes_per_second = 16384;
    limits.byte_burst = 16384;
    return limits;
}

void soak_frames(security::FrameLimiter& frames, Clock::time_point& now, Report& report) {
    for (std::uint32_t index = 0; index < frame_budget; ++index) {
        demand(frames.admit(now), "Stress frame admission drifted");
        ++report.frames_accepted;
    }
    demand(!frames.admit(now), "Stress frame overload drifted");
    ++report.frames_refused;
    now += std::chrono::seconds(1);
}

void release_peer(security::PeerLimiter& peers, const std::string& peer, std::uint32_t count) {
    for (std::uint32_t index = 0; index < count; ++index) { peers.release(peer, true); }
}

void soak_peers(security::PeerLimiter& peers, const std::string& peer, Clock::time_point& now,
                Report& report) {
    for (std::size_t index = 0; index < connections_per_ip; ++index) {
        demand(peers.admit(peer, now), "Stress peer admission drifted");
        ++report.peers_accepted;
    }
    demand(!peers.admit(peer, now), "Stress peer connection overload drifted");
    ++report.peers_refused;
    release_peer(peers, peer, static_cast<std::uint32_t>(connections_per_ip));

    const auto remainder = accepts_per_minute - static_cast<std::uint32_t>(connections_per_ip);
    for (std::uint32_t index = 0; index < remainder; ++index) {
        demand(peers.admit(peer, now), "Stress peer minute admission drifted");
        ++report.peers_accepted;
    }
    demand(!peers.admit(peer, now), "Stress peer minute overload drifted");
    ++report.peers_refused;
    release_peer(peers, peer, remainder);
    demand(peers.pending_logins() == 0, "Stress peer pending drifted");
    demand(peers.tracked_peers() == 1, "Stress peer table drifted");

    now += std::chrono::minutes(1);
    demand(!peers.admit({}, now), "Stress peer prune drifted");
    demand(peers.tracked_peers() == 0, "Stress peer prune drifted");
    demand(peers.pending_logins() == 0, "Stress peer prune drifted");
}

void soak_login(Report& report) {
    security::NegotiationBudget budget{65536, login_frames};
    for (std::uint32_t index = 0; index < login_frames; ++index) {
        demand(budget.admit_frame(), "Stress login frame drifted");
        ++report.login_frames_accepted;
    }
    demand(!budget.admit_frame(), "Stress login overload drifted");
    demand(!budget.admit_frame(), "Stress login failure did not stay failed");
    report.login_frames_refused += 2;
    demand(budget.failed(), "Stress login failure did not stay failed");
}

void soak_chat(chat::FloodGuard& guard, Clock::time_point& now, Report& report) {
    for (std::uint32_t index = 0; index < chat_burst; ++index) {
        demand(guard.admit(1, now).allowed, "Stress chat admission drifted");
        ++report.chat_accepted;
    }
    demand(!guard.admit(1, now).allowed, "Stress chat overload drifted");
    ++report.chat_refused;
    now += std::chrono::seconds(chat_burst);
}

void soak_sessions(users::SessionManager& sessions, users::TimePoint now, std::uint64_t& next_id,
                   Report& report) {
    std::vector<users::SessionId> open;
    open.reserve(session_slots);
    for (std::size_t index = 0; index < session_slots; ++index) {
        const auto id = users::SessionId::from_value(next_id);
        ++next_id;
        demand(id.has_value(), "Stress session id drifted");
        const auto added = sessions.add(*id, users::Transport::adc, now);
        demand(std::holds_alternative<const users::Session*>(added), "Stress session admission drifted");
        open.push_back(*id);
        ++report.sessions_created;
    }
    const auto extra = users::SessionId::from_value(next_id);
    ++next_id;
    demand(extra.has_value(), "Stress session id drifted");
    const auto refused = sessions.add(*extra, users::Transport::adc, now);
    const auto* error = std::get_if<users::RegistryError>(&refused);
    demand(error != nullptr && *error == users::RegistryError::capacity_reached,
           "Stress session overload drifted");
    ++report.sessions_refused;
    for (const auto id : open) {
        demand(!sessions.finish_close(id, users::CloseCause::shutdown, now), "Stress session close drifted");
        demand(!sessions.remove(id), "Stress session removal drifted");
    }
    demand(sessions.size() == 0, "Stress session occupancy drifted");
}

} // namespace

Report run() {
    const auto started = Clock::now();
    // Synthetic time stays ahead of the epoch so every limiter subtracts a
    // forward duration. It is not the process clock and it does not sleep.
    const auto origin = Clock::time_point{std::chrono::hours(1)};
    auto frame_now = origin;
    auto peer_now = origin;
    auto chat_now = origin;
    const std::string peer{"198.51.100.10"};

    security::FrameLimiter frames{frame_budget};
    security::PeerLimiter peers{peer_limits()};
    chat::FloodGuard guard{chat_limits()};
    users::SessionManager sessions{session_slots};
    Report report;
    std::uint64_t next_id = 1;
    for (std::uint64_t round = 0; round < sustained_rounds; ++round) {
        soak_frames(frames, frame_now, report);
        soak_peers(peers, peer, peer_now, report);
        soak_login(report);
        soak_chat(guard, chat_now, report);
        soak_sessions(sessions, origin, next_id, report);
        ++report.rounds;
    }
    const auto statistics = sessions.statistics();
    demand(statistics.created_sessions == report.sessions_created, "Stress session created count drifted");
    demand(statistics.completed_sessions == report.sessions_created, "Stress session completed count drifted");
    report.sessions_completed = statistics.completed_sessions;
    report.tracked_peers = peers.tracked_peers();
    report.pending_logins = peers.pending_logins();
    report.open_sessions = sessions.size();
    demand(report.tracked_peers == 0 && report.pending_logins == 0 && report.open_sessions == 0,
           "Stress occupancy drifted");
    report.elapsed_ms = milliseconds_since(started);
    return report;
}

} // namespace dc24h::stress
