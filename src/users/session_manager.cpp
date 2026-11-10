#include "dc24h/users/session_manager.hpp"

#include <algorithm>
#include <stdexcept>
#include <limits>
#include <utility>

namespace dc24h::users {
namespace {
void increment(std::uint64_t& value) noexcept {
    if (value != std::numeric_limits<std::uint64_t>::max()) { ++value; }
}
std::size_t& phase_count(SessionCounts& counts, Phase phase) noexcept {
    switch (phase) {
    case Phase::accepted: return counts.accepted;
    case Phase::negotiating: return counts.negotiating;
    case Phase::identified: return counts.identified;
    case Phase::ready: return counts.ready;
    case Phase::closing: return counts.closing;
    case Phase::closed: return counts.closed;
    }
    std::terminate();
}
} // namespace
SessionManager::SessionManager(std::size_t capacity) : SessionManager(Limits{capacity,capacity}) {}
SessionManager::SessionManager(Limits limits) : limits_(limits.bounded_by(limits.max_sessions)),
    users_(limits_.max_users), nicknames_(limits_.max_users) {}
Registration SessionManager::add(SessionId id, Transport transport, TimePoint accepted) {
    if (id.value() <= greatest_id_) { return RegistryError::stale_id; }
    if (size() >= capacity()) { return RegistryError::capacity_reached; }
    auto model = std::make_unique<Session>(id, transport, accepted);
    auto* result = model.get();
    sessions_.emplace(id.value(), std::move(model));
    greatest_id_ = id.value();
    ++statistics_.sessions.total;
    ++statistics_.sessions.accepted;
    increment(statistics_.created_sessions);
    return result;
}
Session* SessionManager::mutable_find(SessionId id) noexcept {
    const auto found = sessions_.find(id.value());
    return found == sessions_.end() ? nullptr : found->second.get();
}
const Session* SessionManager::find(SessionId id) const noexcept {
    const auto found = sessions_.find(id.value());
    return found == sessions_.end() ? nullptr : found->second.get();
}
namespace {
MutationResult promote(Result result) noexcept {
    return result ? MutationResult{*result} : std::nullopt;
}
} // namespace
const Session* SessionManager::by_sid(std::string_view sid) const noexcept {
    const auto id = users_.by_sid(sid);
    return id ? find(*id) : nullptr;
}
const Session* SessionManager::by_cid(std::string_view cid) const noexcept {
    const auto id = users_.by_cid(cid);
    return id ? find(*id) : nullptr;
}
const Session* SessionManager::by_nickname(std::string_view nickname) const noexcept {
    const auto id = nicknames_.owner(nickname);
    return id ? find(*id) : nullptr;
}
const User* SessionManager::user_by_sid(std::string_view sid) const noexcept {
    const auto* session = by_sid(sid);
    return session ? &*session->user() : nullptr;
}
const User* SessionManager::user_by_cid(std::string_view cid) const noexcept {
    const auto* session = by_cid(cid);
    return session ? &*session->user() : nullptr;
}
MutationResult SessionManager::start_negotiation(SessionId id, TimePoint now) noexcept {
    return mutate(id,[now](Session& model) { return model.start_negotiation(now); });
}
MutationResult SessionManager::mark_ready(SessionId id, TimePoint now) noexcept {
    return mutate(id,[now](Session& model) { return model.mark_ready(now); });
}
MutationResult SessionManager::begin_close(SessionId id, CloseCause cause, TimePoint now) noexcept {
    return mutate(id,[=](Session& model) { return model.begin_close(cause,now); });
}
MutationResult SessionManager::force_close(SessionId id, CloseCause cause, TimePoint now) noexcept {
    return mutate(id,[=](Session& model) { return model.force_close(cause,now); });
}
MutationResult SessionManager::finish_close(SessionId id, CloseCause cause, TimePoint now) noexcept {
    return mutate(id,[=](Session& model) { return model.finish_close(cause,now); });
}
void SessionManager::erase_identity(SessionId id) noexcept {
    (void)users_.remove(id);
    (void)nicknames_.release(id);
}
MutationResult SessionManager::identify(SessionId id, User user, TimePoint now) {
    auto* model=mutable_find(id);
    if (!model) { return RegistryError::missing; }
    if (model->phase()!=Phase::negotiating) { return SessionError::invalid_state; }
    if (!model->valid_time(now)) { return SessionError::backwards_time; }
    if (!user.valid()) { return SessionError::invalid_identity; }
    const auto& identity=user.identity();
    if (by_sid(identity.sid())) { return RegistryError::duplicate_sid; }
    if (by_cid(identity.cid())) { return RegistryError::duplicate_cid; }
    if (by_nickname(identity.nickname())) { return RegistryError::duplicate_nickname; }
    if (users_full()) { return RegistryError::user_capacity_reached; }
    // Stage allocating indexes before the no-throw model commit. Roll back all
    // successful insertions if any allocation fails; no partial reservations.
    try {
        if (users_.add(id,user)) {
            throw std::logic_error("User registration invariant violated");
        }
        if (nicknames_.reserve(id,identity.nickname_value())) {
            throw std::logic_error("Nickname reservation invariant violated");
        }
    } catch (...) {
        erase_identity(id);
        throw;
    }
    return mutate(id,[&](Session& session) { return session.identify(std::move(user),now); });
}
MutationResult SessionManager::update_user(SessionId id, User user, TimePoint now) {
    auto* model=mutable_find(id);
    if (!model) { return RegistryError::missing; }
    if (model->phase()!=Phase::identified && model->phase()!=Phase::ready) { return SessionError::invalid_state; }
    if (!model->valid_time(now)) { return SessionError::backwards_time; }
    const auto& identity=user.identity();
    if (!user.valid() || identity.sid()!=model->identity()->sid() || identity.cid()!=model->identity()->cid()) {
        return SessionError::invalid_identity;
    }
    if (identity.nickname()==model->identity()->nickname()) { return promote(model->update_user(std::move(user),now)); }
    if (by_nickname(identity.nickname())) { return RegistryError::duplicate_nickname; }
    // Lifecycle, time and identity gates above match Session::update_user.
    // NickManager commits first; the checked model move below cannot throw.
    if (nicknames_.rename(id,identity.nickname_value())) {
        throw std::logic_error("Nickname rename invariant violated");
    }
    return promote(model->update_user(std::move(user),now));
}
MutationResult SessionManager::rename(SessionId id, std::string_view nickname, TimePoint now) {
    const auto* model=find(id);
    if (!model) { return RegistryError::missing; }
    if (model->phase()!=Phase::identified && model->phase()!=Phase::ready) { return SessionError::invalid_state; }
    if (!model->valid_time(now)) { return SessionError::backwards_time; }
    auto checked=Nickname::create(nickname);
    if (!checked) { return SessionError::invalid_identity; }
    auto user=model->user()->with_nickname(std::move(*checked));
    return update_user(id,std::move(*user),now);
}
std::optional<RegistryError> SessionManager::remove(SessionId id) noexcept {
    const auto found = sessions_.find(id.value());
    if (found == sessions_.end()) { return RegistryError::missing; }
    if (found->second->phase() != Phase::closed) { return RegistryError::not_closed; }
    erase_identity(id);
    --statistics_.sessions.total;
    --statistics_.sessions.closed;
    sessions_.erase(found);
    return {};
}
std::optional<Snapshot> SessionManager::snapshot(SessionId id) const {
    const auto* model = find(id);
    return model ? std::optional<Snapshot>{model->snapshot()} : std::nullopt;
}
std::vector<Snapshot> SessionManager::page(std::uint64_t after, std::size_t limit) const {
    if (limit == 0 || limit > max_page_size) { throw std::invalid_argument("Invalid session snapshot page size"); }
    std::vector<Snapshot> result;
    result.reserve(std::min(limit, size()));
    for (auto it = sessions_.upper_bound(after); it != sessions_.end() && result.size() < limit; ++it) {
        result.push_back(it->second->snapshot());
    }
    return result;
}
std::vector<SessionId> SessionManager::public_users(std::uint64_t after, std::size_t limit) const {
    if (limit == 0 || limit > max_page_size) { throw std::invalid_argument("Invalid public user page size"); }
    std::vector<SessionId> result;
    result.reserve(std::min(limit, identified_users()));
    for (auto it = sessions_.upper_bound(after); it != sessions_.end() && result.size() < limit; ++it) {
        if (it->second->phase() == Phase::identified || it->second->phase() == Phase::ready) {
            result.push_back(it->second->id());
        }
    }
    return result;
}
void SessionManager::record_transition(Phase before, Phase after) noexcept {
    if (before == after) { return; }
    --phase_count(statistics_.sessions, before);
    ++phase_count(statistics_.sessions, after);
    if (after == Phase::ready) {
        increment(statistics_.successful_logins);
        statistics_.peak_online = std::max(statistics_.peak_online, statistics_.online());
    }
    if (after == Phase::closed) { increment(statistics_.completed_sessions); }
}
} // namespace dc24h::users
