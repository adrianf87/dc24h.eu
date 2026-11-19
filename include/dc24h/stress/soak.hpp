#pragma once

#include <cstdint>

namespace dc24h::stress {

// Fixed in-process rounds over the existing frame, peer, login, chat and
// session limits. The counts are what those objects already return. They do
// not add a limit, a route, or a capacity claim. configuration.manage and
// hub.manage stay unimplemented.
inline constexpr std::uint64_t sustained_rounds = 24;
inline constexpr std::uint32_t frame_budget = 4;
inline constexpr std::size_t connections_per_ip = 2;
inline constexpr std::uint32_t accepts_per_minute = 3;
inline constexpr std::uint32_t login_frames = 4;
inline constexpr std::uint32_t chat_burst = 2;
inline constexpr std::size_t session_slots = 2;
inline constexpr std::uint64_t stress_ceiling_ms = 5000;

static_assert(sustained_rounds >= 1);
static_assert(frame_budget >= 1);
static_assert(connections_per_ip >= 1);
static_assert(accepts_per_minute > connections_per_ip);
static_assert(login_frames >= 1);
static_assert(chat_burst >= 1);
static_assert(session_slots >= 1);

struct Report {
    std::uint64_t rounds{};
    std::uint64_t frames_accepted{};
    std::uint64_t frames_refused{};
    std::uint64_t peers_accepted{};
    std::uint64_t peers_refused{};
    std::uint64_t login_frames_accepted{};
    std::uint64_t login_frames_refused{};
    std::uint64_t chat_accepted{};
    std::uint64_t chat_refused{};
    std::uint64_t sessions_created{};
    std::uint64_t sessions_completed{};
    std::uint64_t sessions_refused{};
    std::uint64_t tracked_peers{};
    std::uint64_t pending_logins{};
    std::uint64_t open_sessions{};
    std::uint64_t elapsed_ms{};
};

// One frame limiter, one peer limiter, one chat guard and one session table
// stay alive for every round. Each round also builds a fresh login budget,
// because a failed negotiation stays failed. A zero elapsed_ms means the soak
// finished inside one millisecond. The five-second ceiling is a hang guard.
// The function does not open a socket, write a file, or log a peer key.
[[nodiscard]] Report run();

} // namespace dc24h::stress
