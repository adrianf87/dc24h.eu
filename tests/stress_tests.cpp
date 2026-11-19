#include "dc24h/stress/soak.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}

void check_counts(const dc24h::stress::Report& report) {
    constexpr auto rounds = dc24h::stress::sustained_rounds;
    require(report.rounds == rounds, "Stress round count drifted");
    require(report.frames_accepted == rounds * dc24h::stress::frame_budget, "Stress frame accepts drifted");
    require(report.frames_refused == rounds, "Stress frame refusals drifted");
    require(report.peers_accepted == rounds * dc24h::stress::accepts_per_minute, "Stress peer accepts drifted");
    require(report.peers_refused == rounds * 2, "Stress peer refusals drifted");
    require(report.login_frames_accepted == rounds * dc24h::stress::login_frames,
            "Stress login accepts drifted");
    require(report.login_frames_refused == rounds * 2, "Stress login refusals drifted");
    require(report.chat_accepted == rounds * dc24h::stress::chat_burst, "Stress chat accepts drifted");
    require(report.chat_refused == rounds, "Stress chat refusals drifted");
    require(report.sessions_created == rounds * dc24h::stress::session_slots, "Stress sessions created drifted");
    require(report.sessions_completed == report.sessions_created, "Stress sessions completed drifted");
    require(report.sessions_refused == rounds, "Stress session refusals drifted");
    require(report.tracked_peers == 0 && report.pending_logins == 0 && report.open_sessions == 0,
            "Stress occupancy drifted");
    require(report.frames_refused > 0 && report.peers_refused > 0 && report.login_frames_refused > 0 &&
                report.chat_refused > 0 && report.sessions_refused > 0,
            "Stress overload was not observed");
    require(report.elapsed_ms <= dc24h::stress::stress_ceiling_ms, "Stress ceiling exceeded");
}

bool same_work(const dc24h::stress::Report& left, const dc24h::stress::Report& right) {
    return left.rounds == right.rounds && left.frames_accepted == right.frames_accepted &&
           left.frames_refused == right.frames_refused && left.peers_accepted == right.peers_accepted &&
           left.peers_refused == right.peers_refused &&
           left.login_frames_accepted == right.login_frames_accepted &&
           left.login_frames_refused == right.login_frames_refused && left.chat_accepted == right.chat_accepted &&
           left.chat_refused == right.chat_refused && left.sessions_created == right.sessions_created &&
           left.sessions_completed == right.sessions_completed &&
           left.sessions_refused == right.sessions_refused && left.tracked_peers == right.tracked_peers &&
           left.pending_logins == right.pending_logins && left.open_sessions == right.open_sessions;
}

} // namespace

int main() {
    try {
        const auto first = dc24h::stress::run();
        const auto second = dc24h::stress::run();
        check_counts(first);
        check_counts(second);
        require(same_work(first, second), "Stress soak was not repeatable");
        std::cout << "Alpha 0.9.16 stress soak OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
