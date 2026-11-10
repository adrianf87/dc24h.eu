#include "dc24h/users/session_manager.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dc24h::users {
SessionManager::SessionManager(std::size_t capacity) : capacity_(capacity), users_(capacity), nicknames_(capacity) {
    if (capacity == 0 || capacity > max_capacity) { throw std::invalid_argument("Invalid session registry capacity"); }
}
Registration SessionManager::add(SessionId id, Transport transport, TimePoint accepted) {
    if (id.value() <= greatest_id_) { return RegistryError::stale_id; }
    if (size() == capacity_) { return RegistryError::capacity_reached; }
    auto model = std::make_unique<Session>(id, transport, accepted);
    auto* result = model.get();
    sessions_.emplace(id.value(), std::move(model));
    greatest_id_ = id.value();
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
template<class Operation>
MutationResult apply(Session* model, Operation operation) noexcept {
    return model ? promote(operation(*model)) : MutationResult{RegistryError::missing};
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
    return apply(mutable_find(id),[now](Session& model) { return model.start_negotiation(now); });
}
MutationResult SessionManager::mark_ready(SessionId id, TimePoint now) noexcept {
    return apply(mutable_find(id),[now](Session& model) { return model.mark_ready(now); });
}
MutationResult SessionManager::begin_close(SessionId id, CloseCause cause, TimePoint now) noexcept {
    return apply(mutable_find(id),[=](Session& model) { return model.begin_close(cause,now); });
}
MutationResult SessionManager::force_close(SessionId id, CloseCause cause, TimePoint now) noexcept {
    return apply(mutable_find(id),[=](Session& model) { return model.force_close(cause,now); });
}
MutationResult SessionManager::finish_close(SessionId id, CloseCause cause, TimePoint now) noexcept {
    return apply(mutable_find(id),[=](Session& model) { return model.finish_close(cause,now); });
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
    return promote(model->identify(std::move(user),now));
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
SessionCounts SessionManager::counts() const noexcept {
    SessionCounts result;
    result.total = size();
    for (const auto& [id, model] : sessions_) {
        (void)id;
        switch (model->phase()) {
        case Phase::accepted: ++result.accepted; break;
        case Phase::negotiating: ++result.negotiating; break;
        case Phase::identified: ++result.identified; break;
        case Phase::ready: ++result.ready; break;
        case Phase::closing: ++result.closing; break;
        case Phase::closed: ++result.closed; break;
        }
    }
    return result;
}
} // namespace dc24h::users
