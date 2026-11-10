#pragma once

#include "dc24h/users/session.hpp"

#include <map>
#include <utility>

namespace dc24h::users {
enum class UserRegistryError { invalid_user, owner_exists, duplicate_sid, duplicate_cid, capacity_reached };
using UserRegistryResult = std::optional<UserRegistryError>;

// One reactor/thread. Indexes admitted public users without copying their
// profiles or owning Session objects. SessionManager resolves IDs to models.
// SID/CID bindings are immutable until removal; nick/profile changes need no
// reindexing. No borrowed map iterators or mutable models escape.
class UserRegistry final {
public:
    static constexpr std::size_t max_capacity = 65535;
    explicit UserRegistry(std::size_t capacity);
    UserRegistry(const UserRegistry&) = delete;
    UserRegistry& operator=(const UserRegistry&) = delete;
    UserRegistry(UserRegistry&&) = delete;
    UserRegistry& operator=(UserRegistry&&) = delete;
    [[nodiscard]] UserRegistryResult add(SessionId owner, const User& user);
    [[nodiscard]] std::optional<SessionId> by_sid(std::string_view sid) const noexcept;
    [[nodiscard]] std::optional<SessionId> by_cid(std::string_view cid) const noexcept;
    [[nodiscard]] bool contains(SessionId owner) const noexcept;
    // Release both keys by local lifetime ID, never by a reusable public key.
    // Closing/finalization alone must not call this; SessionManager removes it.
    [[nodiscard]] bool remove(SessionId owner) noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return owners_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
private:
    using Index = std::map<std::string, SessionId, std::less<>>;
    [[nodiscard]] static std::optional<SessionId> lookup(const Index& index,
        std::string_view key, std::size_t maximum) noexcept;
    std::size_t capacity_;
    Index sids_, cids_;
    std::map<std::uint64_t, std::pair<Index::iterator, Index::iterator>> owners_;
};
} // namespace dc24h::users
