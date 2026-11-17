#include "dc24h/users/session.hpp"

#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dc24h::users {
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
Session::Session(SessionId id, Transport transport, TimePoint accepted) noexcept
    : id_(id), transport_(transport), timeline_{accepted,{},{},{},{},{}}, last_(accepted) {}
Result Session::start_negotiation(TimePoint now) noexcept {
    if (phase_ != Phase::accepted) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    timeline_.negotiating=now;last_=now;phase_=Phase::negotiating;return {};
}
Result Session::identify(Identity identity, TimePoint now) noexcept {
    auto user=User::create(std::move(identity));
    if (!user) { return SessionError::invalid_identity; }
    return identify(std::move(*user),now);
}
Result Session::identify(User user, TimePoint now) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<std::optional<User>>);
    if (phase_ != Phase::negotiating) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    if (!user.valid()) { return SessionError::invalid_identity; }
    user_=std::move(user);timeline_.identified=now;last_=now;phase_=Phase::identified;return {};
}
Result Session::update_user(User user, TimePoint now) noexcept {
    if (phase_ != Phase::identified && phase_ != Phase::ready) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    if (!user.valid() || user.identity().sid()!=identity()->sid() || user.identity().cid()!=identity()->cid() ||
        (authentication_ && user.identity().nickname()!=identity()->nickname())) {
        return SessionError::invalid_identity;
    }
    user_=std::move(user);last_=now;return {};
}
Result Session::mark_ready(TimePoint now) noexcept {
    if (phase_ != Phase::identified) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    timeline_.ready=now;last_=now;phase_=Phase::ready;return {};
}
Result Session::rename(std::string_view nickname, TimePoint now) {
    if (phase_ != Phase::identified && phase_ != Phase::ready) { return SessionError::invalid_state; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    auto checked=Nickname::create(nickname);
    if (!checked) { return SessionError::invalid_identity; }
    auto candidate=user_->with_nickname(std::move(*checked));
    if (!candidate) { return SessionError::invalid_identity; }
    return update_user(std::move(*candidate),now);
}
Result Session::set_chat_mute(ChatMute state,TimePoint now) noexcept {
    if(!ready()){return SessionError::invalid_state;}
    if(!valid_time(now)){return SessionError::backwards_time;}
    chat_mute_=state;last_=now;return {};
}
Result Session::begin_close(CloseCause cause, TimePoint now) noexcept {
    if (phase_ == Phase::closing || phase_ == Phase::closed) { return {}; }
    if (!valid_time(now)) { return SessionError::backwards_time; }
    authentication_.reset();chat_mute_={};
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
    return {id_,transport_,phase_,timeline_,
        user_ ? std::optional<Identity>{user_->identity()} : std::nullopt,
        user_ ? std::optional<Profile>{user_->profile()} : std::nullopt,
        close_cause_,forced_cause_,final_cause_,close_mode_,authentication_,chat_mute_};
}
} // namespace dc24h::users
