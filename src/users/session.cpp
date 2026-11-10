#include "dc24h/users/session.hpp"

#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dc24h::users {
namespace {
bool bounded(std::string_view value, std::size_t maximum) noexcept {
    return !value.empty() && value.size() <= maximum && value.find('\0') == std::string_view::npos;
}
} // namespace
std::optional<SessionId> SessionId::from_value(std::uint64_t value) noexcept {
    return value == 0 ? std::nullopt : std::optional<SessionId>{SessionId(value)};
}
SessionIds::SessionIds(std::uint64_t first) : next_(first) {
    if (first == 0) { throw std::invalid_argument("Session IDs start above zero"); }
}
std::optional<SessionId> SessionIds::acquire() noexcept {
    if (exhausted_) { return std::nullopt; }
    const auto result = SessionId::from_value(next_);
    if (next_ == std::numeric_limits<std::uint64_t>::max()) { exhausted_ = true; }
    else { ++next_; }
    return result;
}
Identity::Identity(std::string_view sid, std::string_view cid, std::string_view nickname)
    : sid_(sid), cid_(cid), nickname_(nickname) {}
std::optional<Identity> Identity::create(std::string_view sid, std::string_view cid, std::string_view nickname) {
    if (!bounded(sid,max_session_key_bytes) || !bounded(cid,max_client_key_bytes) || !bounded(nickname,max_nickname_bytes)) { return std::nullopt; }
    return Identity(sid,cid,nickname);
}
Session::Session(SessionId id, Transport transport, TimePoint accepted) noexcept
    : id_(id), transport_(transport), timeline_{accepted,{},{},{},{},{}}, last_(accepted) {}
Result Session::start_negotiation(TimePoint now) noexcept {
    if (phase_ != Phase::accepted) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    timeline_.negotiating=now;last_=now;phase_=Phase::negotiating;return {};
}
Result Session::identify(Identity identity, TimePoint now) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<std::optional<Identity>>);
    if (phase_ != Phase::negotiating) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    if (!bounded(identity.sid(),max_session_key_bytes) || !bounded(identity.cid(),max_client_key_bytes) ||
        !bounded(identity.nickname(),max_nickname_bytes)) {
        return SessionError::invalid_identity;
    }
    identity_=std::move(identity);timeline_.identified=now;last_=now;phase_=Phase::identified;return {};
}
Result Session::mark_ready(TimePoint now) noexcept {
    if (phase_ != Phase::identified) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    timeline_.ready=now;last_=now;phase_=Phase::ready;return {};
}
Result Session::rename(std::string_view nickname, TimePoint now) {
    if (phase_ != Phase::identified && phase_ != Phase::ready) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    if (!bounded(nickname,max_nickname_bytes)) { return SessionError::invalid_identity; }
    std::string owned(nickname);identity_->nickname_.swap(owned);last_=now;return {};
}
Result Session::begin_close(CloseCause cause, TimePoint now) noexcept {
    if (phase_ == Phase::closing || phase_ == Phase::closed) { return {}; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    close_cause_=cause;timeline_.closing=now;last_=now;phase_=Phase::closing;return {};
}
Result Session::force_close(CloseCause cause, TimePoint now) noexcept {
    if (phase_ == Phase::closed) { return {}; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    if (const auto error=begin_close(cause,now)) { return error; }
    if (!forced_cause_) { forced_cause_=cause; }
    close_mode_=CloseMode::immediate;last_=now;return {};
}
Result Session::finish_close(CloseCause cause, TimePoint now) noexcept {
    if (phase_ == Phase::closed) { return {}; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    if (phase_ != Phase::closing) {
        if (const auto error=force_close(cause,now)) { return error; }
    }
    final_cause_=cause;timeline_.closed=now;last_=now;phase_=Phase::closed;return {};
}
std::optional<CloseCause> Session::terminal_cause() const noexcept {
    return forced_cause_ ? forced_cause_ : close_cause_;
}
Snapshot Session::snapshot() const {
    return {id_,transport_,phase_,timeline_,identity_,close_cause_,forced_cause_,final_cause_,close_mode_};
}
} // namespace dc24h::users
