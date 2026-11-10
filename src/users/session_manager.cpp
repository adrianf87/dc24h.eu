#include "dc24h/users/session_manager.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dc24h::users {
SessionManager::SessionManager(std::size_t capacity) : capacity_(capacity) {
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
const Session* SessionManager::indexed(const Index& index, std::string_view key, std::size_t maximum) const noexcept {
    if (key.empty() || key.size()>maximum || key.find('\0')!=std::string_view::npos) { return nullptr; }
    const auto found=index.find(key);
    return found==index.end() ? nullptr : find(found->second);
}
const Session* SessionManager::by_sid(std::string_view sid) const noexcept { return indexed(sids_,sid,max_session_key_bytes); }
const Session* SessionManager::by_cid(std::string_view cid) const noexcept { return indexed(cids_,cid,max_client_key_bytes); }
const Session* SessionManager::by_nickname(std::string_view nickname) const noexcept { return indexed(nicknames_,nickname,max_nickname_bytes); }
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
void SessionManager::erase_identity(SessionId id, const Identity& identity) noexcept {
    const auto erase_owned=[id](Index& index, std::string_view key) {
        const auto found=index.find(key);
        if (found!=index.end() && found->second==id) { index.erase(found); }
    };
    erase_owned(sids_,identity.sid());erase_owned(cids_,identity.cid());erase_owned(nicknames_,identity.nickname());
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
        sids_.emplace(identity.sid(),id);
        cids_.emplace(identity.cid(),id);
        nicknames_.emplace(identity.nickname(),id);
    } catch (...) {
        erase_identity(id,identity);
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
    const auto old=nicknames_.find(model->identity()->nickname());
    // std::map keeps old iterators valid while the new key is allocated. User
    // move assignment and erasing the old key cannot throw after staging.
    nicknames_.emplace(identity.nickname(),id);
    const auto result=promote(model->update_user(std::move(user),now));
    nicknames_.erase(old);
    return result;
}
MutationResult SessionManager::rename(SessionId id, std::string_view nickname, TimePoint now) {
    const auto* model=find(id);
    if (!model) { return RegistryError::missing; }
    if (model->phase()!=Phase::identified && model->phase()!=Phase::ready) { return SessionError::invalid_state; }
    if (!model->valid_time(now)) { return SessionError::backwards_time; }
    auto identity=Identity::create(model->identity()->sid(),model->identity()->cid(),nickname);
    if (!identity) { return SessionError::invalid_identity; }
    auto user=User::create(std::move(*identity),model->user()->profile());
    return update_user(id,std::move(*user),now);
}
std::optional<RegistryError> SessionManager::remove(SessionId id) noexcept {
    const auto found = sessions_.find(id.value());
    if (found == sessions_.end()) { return RegistryError::missing; }
    if (found->second->phase() != Phase::closed) { return RegistryError::not_closed; }
    if (found->second->identity()) { erase_identity(id,*found->second->identity()); }
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
