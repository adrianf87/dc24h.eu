#pragma once

#include "dc24h/adc/parser.hpp"
#include "dc24h/transfers/core.hpp"
#include <array>
#include <span>

namespace dc24h::adc {
enum class ConnectionRoutingError { invalid_state, wrong_source, invalid_message, recipient_unavailable,
    inactive_sender, foreign_plan, missing_sink, already_dispatched, incompatible_connectivity };
namespace detail { struct ConnectionDomain final {}; }
class ConnectionFrame final {
public:
    ConnectionFrame(const ConnectionFrame&) = delete;
    ConnectionFrame& operator=(const ConnectionFrame&) = delete;
    ConnectionFrame(ConnectionFrame&&) = delete;
    ConnectionFrame& operator=(ConnectionFrame&&) = delete;
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
private:
    friend class ConnectionRouter;
    ConnectionFrame(Message message, std::string wire) : message_(std::move(message)), wire_(std::move(wire)) {}
    Message message_;
    std::string wire_;
};
using SharedConnectionFrame = std::shared_ptr<const ConnectionFrame>;
class RoutedConnection final {
public:
    [[nodiscard]] const transfers::Plan& plan() const noexcept { return plan_; }
    [[nodiscard]] const SharedConnectionFrame& frame() const noexcept { return frame_; }
    // Fixed target-first lifetime audience; dispatch may cancel a stale copy,
    // never add a bystander, new login or replacement for a reused SID.
    [[nodiscard]] std::span<const users::SessionId> recipients() const noexcept { return {recipients_.data(), count_}; }
private:
    friend class ConnectionRouter;
    RoutedConnection(std::shared_ptr<const detail::ConnectionDomain> domain, transfers::Plan plan, SharedConnectionFrame frame)
        : domain_(std::move(domain)), plan_(std::move(plan)), frame_(std::move(frame)),
          recipients_{plan_.target(), plan_.sender()}, count_(frame_->message().type == MessageType::echo ? 2U : 1U) {}
    std::shared_ptr<const detail::ConnectionDomain> domain_;
    transfers::Plan plan_;
    SharedConnectionFrame frame_;
    std::array<users::SessionId, 2> recipients_;
    std::size_t count_;
};
struct ConnectionDispatchReport {
    std::size_t queued{}, refused{}, canceled{};
    friend bool operator==(const ConnectionDispatchReport&, const ConnectionDispatchReport&) = default;
};
using ConnectionPreparation = std::variant<RoutedConnection, ConnectionRoutingError>;
using ConnectionDispatch = std::variant<ConnectionDispatchReport, ConnectionRoutingError>;
// Single owner-thread admission and delivery for D/E CTM/RCM only. Transfer
// Core and SessionManager must outlive this router and all dispatch calls.
// Tokens never select recipients; public SID keys resolve to ready lifetimes.
class ConnectionRouter final {
public:
    using Sink = std::function<bool(users::SessionId, const SharedConnectionFrame&)>;
    explicit ConnectionRouter(transfers::Core& core) : core_(core) {}
    ConnectionRouter(const ConnectionRouter&) = delete;
    ConnectionRouter& operator=(const ConnectionRouter&) = delete;
    [[nodiscard]] ConnectionPreparation prepare(users::SessionId sender, const Message& message);
    // All command types use one engine/domain and one consumed Core attempt.
    // No allocations after sink construction, including echo and rechecks.
    // Queue acceptance is not client delivery or connection establishment.
    [[nodiscard]] ConnectionDispatch dispatch(const RoutedConnection& message, const Sink& sink);
    [[nodiscard]] bool cancel(const RoutedConnection& message) noexcept;
private:
    friend class CtmRouter;
    friend class RcmRouter;
    // Legacy typed adapters restrict admission while sharing implementation.
    ConnectionRouter(transfers::Core& core, Command accepted) : core_(core), accepted_(accepted) {}
    [[nodiscard]] ConnectionPreparation admit(users::SessionId sender, const Message& checked, const transfers::RequestData& data);
    transfers::Core& core_;
    Command accepted_{Command::unknown};
    const std::shared_ptr<const detail::ConnectionDomain> domain_{std::make_shared<const detail::ConnectionDomain>()};
};
} // namespace dc24h::adc
