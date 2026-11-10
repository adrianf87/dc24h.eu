#pragma once

#include "dc24h/adc/inf.hpp"
#include "dc24h/users/session_manager.hpp"

namespace dc24h::adc {
enum class InfUpdateError { identity_mismatch, invalid_public_user, invalid_output };
using InfUpdateFailure = std::variant<ClientSessionError, users::RegistryError, users::SessionError, InfUpdateError>;
struct AppliedInfUpdate {
    // Unknown-only/empty input has no public message. Explicit unchanged public
    // values are still echoed; cancellations are bare known field names.
    std::optional<Message> message;
};
using InfUpdateResult = std::variant<AppliedInfUpdate, InfUpdateFailure>;
[[nodiscard]] std::string_view inf_update_status(const InfUpdateFailure& error) noexcept;

// One reactor/thread. Validate and build public delta before committing the
// managed User/nickname index and ClientInfo together. No socket/output I/O.
// Caller supplies the ClientSession paired with this local session ID.
class InfUpdates final {
public:
    explicit InfUpdates(users::SessionManager& sessions) noexcept : sessions_(sessions) {}
    InfUpdates(const InfUpdates&) = delete;
    InfUpdates& operator=(const InfUpdates&) = delete;
    InfUpdates(InfUpdates&&) = delete;
    InfUpdates& operator=(InfUpdates&&) = delete;
    [[nodiscard]] InfUpdateResult apply(users::SessionId id, ClientSession& client,
        const Message& input, users::TimePoint now);
private:
    users::SessionManager& sessions_;
};
} // namespace dc24h::adc
