#pragma once

#include "dc24h/users/user.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace dc24h::users {
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
// Local lifetime identity, independent of the ADC SID, fd, CID or account.
class SessionId final {
public:
    [[nodiscard]] static std::optional<SessionId> from_value(std::uint64_t value) noexcept;
    [[nodiscard]] std::uint64_t value() const noexcept { return value_; }
    friend bool operator==(SessionId, SessionId) = default;
private:
    explicit SessionId(std::uint64_t value) noexcept : value_(value) {}
    std::uint64_t value_;
};
// One sequence per reactor lifetime. Exhaustion never wraps/reuses an old ID.
class SessionIds final {
public:
    explicit SessionIds(std::uint64_t first = 1);
    SessionIds(const SessionIds&) = delete;
    SessionIds& operator=(const SessionIds&) = delete;
    SessionIds(SessionIds&&) = delete;
    SessionIds& operator=(SessionIds&&) = delete;
    [[nodiscard]] std::optional<SessionId> acquire() noexcept;
private:
    std::uint64_t next_;
    bool exhausted_{false};
};
enum class Transport { adc, adcs };
enum class Phase { accepted, negotiating, identified, ready, closing, closed };
enum class CloseCause {
    protocol_rejected, peer_status, peer_closed, tls_handshake_failed, tls_closed,
    tls_read_error, tcp_read_error, socket_error, write_error, shutdown,
    login_timeout, idle_timeout, handshake_timeout, partial_frame_timeout,
    output_timeout, output_limit, security_quota, negotiation_limit,
    identity_limit, close_timeout, internal_error
};
enum class CloseMode { drain, immediate };
enum class SessionError { invalid_state, backwards_time, invalid_identity };
using Result = std::optional<SessionError>;

struct Timeline {
    TimePoint accepted;
    std::optional<TimePoint> negotiating, identified, ready, closing, closed;
    friend bool operator==(const Timeline&, const Timeline&) = default;
};
struct Snapshot {
    SessionId id;
    Transport transport;
    Phase phase;
    Timeline timeline;
    std::optional<Identity> identity;
    std::optional<Profile> profile;
    std::optional<CloseCause> close_cause;
    std::optional<CloseCause> forced_cause;
    std::optional<CloseCause> final_cause;
    CloseMode close_mode;
    friend bool operator==(const Snapshot&, const Snapshot&) = default;
};

// Single owner/thread; no sockets, protocol parser, database, credentials,
// logger or clocks read internally. Failed transitions leave the model intact.
// Closing/finish are idempotent; escalation never restarts the close deadline.
class Session final {
public:
    Session(SessionId id, Transport transport, TimePoint accepted) noexcept;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;
    [[nodiscard]] Result start_negotiation(TimePoint now) noexcept;
    [[nodiscard]] Result identify(Identity identity, TimePoint now) noexcept;
    [[nodiscard]] Result identify(User user, TimePoint now) noexcept;
    [[nodiscard]] Result update_user(User user, TimePoint now) noexcept;
    [[nodiscard]] Result mark_ready(TimePoint now) noexcept;
    [[nodiscard]] Result rename(std::string_view nickname, TimePoint now);
    [[nodiscard]] Result begin_close(CloseCause cause, TimePoint now) noexcept;
    [[nodiscard]] Result force_close(CloseCause cause, TimePoint now) noexcept;
    [[nodiscard]] Result finish_close(CloseCause cause, TimePoint now) noexcept;
    [[nodiscard]] SessionId id() const noexcept { return id_; }
    [[nodiscard]] Transport transport() const noexcept { return transport_; }
    [[nodiscard]] Phase phase() const noexcept { return phase_; }
    [[nodiscard]] const Timeline& timeline() const noexcept { return timeline_; }
    [[nodiscard]] const Identity* identity() const noexcept { return user_ ? &user_->identity() : nullptr; }
    [[nodiscard]] const std::optional<User>& user() const noexcept { return user_; }
    [[nodiscard]] bool was_ready() const noexcept { return timeline_.ready.has_value(); }
    [[nodiscard]] bool ready() const noexcept { return phase_ == Phase::ready; }
    [[nodiscard]] bool closing() const noexcept { return phase_ == Phase::closing; }
    [[nodiscard]] bool immediate() const noexcept { return close_mode_ == CloseMode::immediate; }
    [[nodiscard]] std::optional<CloseCause> terminal_cause() const noexcept;
    [[nodiscard]] Snapshot snapshot() const;
private:
    [[nodiscard]] bool valid_time(TimePoint now) const noexcept { return now >= last_; }
    SessionId id_;
    Transport transport_;
    Phase phase_{Phase::accepted};
    Timeline timeline_;
    TimePoint last_;
    std::optional<User> user_;
    std::optional<CloseCause> close_cause_, forced_cause_, final_cause_;
    CloseMode close_mode_{CloseMode::drain};
};
} // namespace dc24h::users
