#pragma once

#include "dc24h/adc/ctm.hpp"
#include "dc24h/transfers/core.hpp"

namespace dc24h::adc {
enum class CtmRoutingError { invalid_state, wrong_source, invalid_message, recipient_unavailable,
    inactive_sender, foreign_plan, missing_sink, already_dispatched };
namespace detail { struct CtmDomain final {}; }
class CtmFrame final {
public:
    CtmFrame(const CtmFrame&) = delete;
    CtmFrame& operator=(const CtmFrame&) = delete;
    CtmFrame(CtmFrame&&) = delete;
    CtmFrame& operator=(CtmFrame&&) = delete;
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
private:
    friend class CtmRouter;
    CtmFrame(Message message, std::string wire) : message_(std::move(message)), wire_(std::move(wire)) {}
    Message message_;
    std::string wire_;
};
using SharedCtmFrame = std::shared_ptr<const CtmFrame>;
class RoutedCtm final {
public:
    [[nodiscard]] const transfers::Plan& plan() const noexcept { return plan_; }
    [[nodiscard]] const SharedCtmFrame& frame() const noexcept { return frame_; }
private:
    friend class CtmRouter;
    RoutedCtm(std::shared_ptr<const detail::CtmDomain> domain, transfers::Plan plan, SharedCtmFrame frame)
        : domain_(std::move(domain)), plan_(std::move(plan)), frame_(std::move(frame)) {}
    std::shared_ptr<const detail::CtmDomain> domain_;
    transfers::Plan plan_;
    SharedCtmFrame frame_;
};
struct CtmDispatchReport {
    std::size_t queued{}, refused{}, canceled{};
    friend bool operator==(const CtmDispatchReport&, const CtmDispatchReport&) = default;
};
// Owner-thread DCTM / ECTM over Transfer Core. One shared immutable frame, no
// socket/file I/O. Unknown valid protocol names remain a client negotiation.
class CtmRouter final {
public:
    using Sink = std::function<bool(users::SessionId, const SharedCtmFrame&)>;
    explicit CtmRouter(transfers::Core& core) : core_(core) {}
    CtmRouter(const CtmRouter&) = delete;
    CtmRouter& operator=(const CtmRouter&) = delete;
    [[nodiscard]] std::variant<RoutedCtm, CtmRoutingError> prepare(users::SessionId sender, const Message& message);
    // One attempt across copies, including exceptions. No allocation after sink
    // creation. E echo is an independent queue copy, not a delivery acknowledgment.
    [[nodiscard]] std::variant<CtmDispatchReport, CtmRoutingError> dispatch(const RoutedCtm& message, const Sink& sink);
    [[nodiscard]] bool cancel(const RoutedCtm& message) noexcept;
private:
    transfers::Core& core_;
    const std::shared_ptr<const detail::CtmDomain> domain_{std::make_shared<const detail::CtmDomain>()};
};
} // namespace dc24h::adc
