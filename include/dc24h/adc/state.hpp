#pragma once

#include "dc24h/adc/parser.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace dc24h::adc {

// Hub side of one ADC/ADCS connection; unrelated to the Core lifecycle.
// BASE hub connections have no DATA state (binary transfers require an extension).
enum class ConnectionState { protocol, identify, verify, normal, disconnected };
enum class Direction { from_client, to_client };
enum class IdentityMode { guest, password_required };
enum class StateError {
    disconnected, invalid_message, invalid_type, invalid_state,
    invalid_sequence, wrong_source, unsupported_command
};
using StateResult = std::optional<StateError>; // nullopt = accepted

// Transport-independent, single-owner/thread, noncopyable and nonmovable.
// Only parsed messages may enter observe(). No payloads, PID/PAS, capabilities,
// users or credentials are retained. Rejection leaves all state unchanged.
class ConnectionStateMachine final {
public:
    ConnectionStateMachine() = default;
    ConnectionStateMachine(const ConnectionStateMachine&) = delete;
    ConnectionStateMachine& operator=(const ConnectionStateMachine&) = delete;
    ConnectionStateMachine(ConnectionStateMachine&&) = delete;
    ConnectionStateMachine& operator=(ConnectionStateMachine&&) = delete;

    [[nodiscard]] ConnectionState state() const noexcept { return state_; }
    [[nodiscard]] std::string_view sid() const noexcept { return sid_; }
    // to_client means a complete, ordered send, NOT merely an enqueued reply.
    // Preflight outbound syntax/permissions in the future dispatcher before I/O.
    [[nodiscard]] StateResult observe(Direction direction, const Message& message);
    [[nodiscard]] StateResult keepalive() const noexcept;

    // Explicit application decisions; these methods do NOT perform validation.
    // Call only after successful feature/hash, identity or password checks.
    [[nodiscard]] StateResult approve_protocol() noexcept;
    [[nodiscard]] StateResult approve_identity(IdentityMode mode) noexcept;
    [[nodiscard]] StateResult approve_password() noexcept;
    // EOF, timeout, rejection or failed send. Terminal, idempotent; no reset.
    void disconnect() noexcept;

private:
    ConnectionState state_{ConnectionState::protocol};
    std::string sid_;
    bool client_sup_{false};
    bool protocol_approved_{false};
    bool hub_sup_{false};
    bool identity_pending_{false};
    bool challenge_sent_{false};
    bool password_pending_{false};
};

} // namespace dc24h::adc
