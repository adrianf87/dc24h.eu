#pragma once

#include "dc24h/adc/sid.hpp"
#include "dc24h/users/session_manager.hpp"

namespace dc24h::adc {
struct Departure {
    users::SessionId id;
    std::optional<Sid> public_sid;
    bool first;
    bool was_ready;
    bool invalid_public_sid;
};
using DepartureResult = std::variant<Departure, users::RegistryError, users::SessionError>;

// One reactor/thread, no allocations, sockets, queues or clock reads. Finalize
// first, then let the owner destroy protocol/output references, then release.
// The copied SID remains usable for public QUI after all source data is gone.
class UserDisconnect final {
public:
    explicit UserDisconnect(users::SessionManager& sessions) noexcept : sessions_(sessions) {}
    UserDisconnect(const UserDisconnect&) = delete;
    UserDisconnect& operator=(const UserDisconnect&) = delete;
    UserDisconnect(UserDisconnect&&) = delete;
    UserDisconnect& operator=(UserDisconnect&&) = delete;
    [[nodiscard]] DepartureResult begin(users::SessionId id, users::CloseCause cause,
        users::TimePoint now) noexcept;
    // Caller must drop borrowed Session references before releasing its model.
    [[nodiscard]] std::optional<users::RegistryError> release(users::SessionId id) noexcept;
private:
    users::SessionManager& sessions_;
};
} // namespace dc24h::adc
