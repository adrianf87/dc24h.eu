#pragma once

#include "dc24h/adc/inf.hpp"
#include "dc24h/users/session_manager.hpp"

#include <functional>
#include <utility>
#include <vector>

namespace dc24h::adc {
enum class UserListError { invalid_state, invalid_identity, inconsistent_source, output_limit };

// Owned, preflighted welcome: hub INF, existing users by local SessionId, own
// INF last. Only the output owner may mark that final frame as a completion.
class InitialUserList final {
public:
    [[nodiscard]] const std::vector<Message>& frames() const noexcept { return frames_; }
    [[nodiscard]] std::size_t wire_bytes() const noexcept { return bytes_; }
private:
    friend class UserList;
    InitialUserList(std::vector<Message> frames, std::size_t bytes)
        : frames_(std::move(frames)), bytes_(bytes) {}
    std::vector<Message> frames_;
    std::size_t bytes_;
};
using UserListResult = std::variant<InitialUserList, UserListError>;

// Single owner/thread, read-only during prepare. The trusted lookup borrows
// the matching peer's validated public ClientInfo and must not mutate state.
// No persistent duplicate user store, clocks, sockets or output queue here.
class UserList final {
public:
    using InfoLookup = std::function<const ClientInfo*(users::SessionId)>;
    explicit UserList(const users::SessionManager& sessions) noexcept : sessions_(sessions) {}
    UserList(const UserList&) = delete;
    UserList& operator=(const UserList&) = delete;
    UserList(UserList&&) = delete;
    UserList& operator=(UserList&&) = delete;
    [[nodiscard]] UserListResult prepare(users::SessionId joining, const ClientSession& client,
        const InfoLookup& lookup, std::size_t available_wire_bytes) const;
private:
    const users::SessionManager& sessions_;
};
} // namespace dc24h::adc
