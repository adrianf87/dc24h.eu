#pragma once

#include "dc24h/users/session.hpp"

#include <map>

namespace dc24h::users {
enum class NickError { invalid_nickname, duplicate_nickname, owner_exists, missing, capacity_reached };
using NickResult = std::optional<NickError>;

// One reactor/thread; one checked name per session, one session per exact name.
// SessionManager owns this registry and controls lifecycle/transaction gates.
// Failed mutations (including allocation failure) preserve both indexes.
class NickManager final {
public:
    static constexpr std::size_t max_capacity = 65535;
    explicit NickManager(std::size_t capacity);
    NickManager(const NickManager&) = delete;
    NickManager& operator=(const NickManager&) = delete;
    NickManager(NickManager&&) = delete;
    NickManager& operator=(NickManager&&) = delete;
    [[nodiscard]] NickResult reserve(SessionId owner, std::string_view nickname);
    [[nodiscard]] NickResult reserve(SessionId owner, const Nickname& nickname);
    [[nodiscard]] NickResult rename(SessionId owner, std::string_view nickname);
    [[nodiscard]] NickResult rename(SessionId owner, const Nickname& nickname);
    [[nodiscard]] std::optional<SessionId> owner(std::string_view nickname) const noexcept;
    // Explicit cleanup by lifetime ID; an old owner cannot release a reused name.
    [[nodiscard]] bool release(SessionId owner) noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return names_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
private:
    using Names = std::map<std::string, SessionId, std::less<>>;
    std::size_t capacity_;
    Names names_;
    std::map<std::uint64_t, Names::iterator> owners_;
};
} // namespace dc24h::users
