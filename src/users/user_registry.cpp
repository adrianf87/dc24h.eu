#include "dc24h/users/user_registry.hpp"

#include <stdexcept>

namespace dc24h::users {
UserRegistry::UserRegistry(std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0 || capacity > max_capacity) {
        throw std::invalid_argument("Invalid user registry capacity");
    }
}
std::optional<SessionId> UserRegistry::lookup(const Index& index, std::string_view key,
        std::size_t maximum) noexcept {
    if (key.empty() || key.size() > maximum || key.find('\0') != std::string_view::npos) {
        return std::nullopt;
    }
    const auto found = index.find(key);
    return found == index.end() ? std::nullopt : std::optional<SessionId>{found->second};
}
std::optional<SessionId> UserRegistry::by_sid(std::string_view sid) const noexcept {
    return lookup(sids_, sid, max_session_key_bytes);
}
std::optional<SessionId> UserRegistry::by_cid(std::string_view cid) const noexcept {
    return lookup(cids_, cid, max_client_key_bytes);
}
bool UserRegistry::contains(SessionId owner) const noexcept { return owners_.contains(owner.value()); }
UserRegistryResult UserRegistry::add(SessionId owner, const User& user) {
    if (!user.valid()) { return UserRegistryError::invalid_user; }
    if (contains(owner)) { return UserRegistryError::owner_exists; }
    const auto& identity = user.identity();
    if (by_sid(identity.sid())) { return UserRegistryError::duplicate_sid; }
    if (by_cid(identity.cid())) { return UserRegistryError::duplicate_cid; }
    if (size() == capacity_) { return UserRegistryError::capacity_reached; }

    // Allocate all three nodes before publishing a complete registration.
    // The owning thread cannot observe an intermediate state. Each failure
    // erases only this call's successfully inserted nodes, without allocating.
    const auto sid = sids_.emplace(identity.sid(), owner).first;
    try {
        const auto cid = cids_.emplace(identity.cid(), owner).first;
        try {
            owners_.emplace(owner.value(), std::make_pair(sid, cid));
        } catch (...) {
            cids_.erase(cid);
            throw;
        }
    } catch (...) {
        sids_.erase(sid);
        throw;
    }
    return {};
}
bool UserRegistry::remove(SessionId owner) noexcept {
    const auto found = owners_.find(owner.value());
    if (found == owners_.end()) { return false; }
    sids_.erase(found->second.first);
    cids_.erase(found->second.second);
    owners_.erase(found);
    return true;
}
} // namespace dc24h::users
