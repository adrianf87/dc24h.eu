#pragma once

#include "dc24h/adc/sid.hpp"
#include "dc24h/adc/sup.hpp"

namespace dc24h::adc {

enum class SidError { invalid_state, invalid_sequence, exhausted, not_reserved, reply_mismatch };
[[nodiscard]] std::string_view sid_status_code(SidError error) noexcept;
using SidSessionError = std::variant<StateError, SupportError, SidError>;
using SidSessionResult = std::optional<SidSessionError>;

// Composes a shared hub SID pool with SUP/state sequencing. No sockets, output
// queue or identity validation. One owner thread for this session and its pool.
class SidSession final {
public:
    // Non-null shared ownership keeps the allocator alive through this session.
    explicit SidSession(std::shared_ptr<SidPool> pool, SupportPolicy policy = {});
    SidSession(const SidSession&) = delete;
    SidSession& operator=(const SidSession&) = delete;
    SidSession(SidSession&&) = delete;
    SidSession& operator=(SidSession&&) = delete;
    [[nodiscard]] SidSessionResult observe(Direction direction, const Message& message);
    // Call after completed ISUP, before preparing/sending ISID. Exhaustion has
    // no side effects; no state transition occurs until the ISID send completes.
    [[nodiscard]] std::optional<SidError> prepare_sid() noexcept;
    [[nodiscard]] std::optional<Sid> reserved_sid() const noexcept { return lease_.sid(); }
    [[nodiscard]] std::optional<Message> sid_reply() const;
    [[nodiscard]] std::optional<std::string> sid_reply_line() const;
    [[nodiscard]] ConnectionState state() const noexcept { return protocol_.state(); }
    [[nodiscard]] std::string_view sid() const noexcept { return protocol_.sid(); }
    [[nodiscard]] const SupportNegotiation& support() const noexcept { return protocol_.support(); }
    [[nodiscard]] StateResult keepalive() const noexcept { return protocol_.keepalive(); }
    [[nodiscard]] StateResult approve_identity(IdentityMode mode) noexcept { return protocol_.approve_identity(mode); }
    [[nodiscard]] StateResult approve_password() noexcept { return protocol_.approve_password(); }
    void disconnect() noexcept;
private:
    std::shared_ptr<SidPool> pool_;
    ProtocolConnection protocol_;
    SidLease lease_;
    bool sup_sent_{false};
};

} // namespace dc24h::adc
