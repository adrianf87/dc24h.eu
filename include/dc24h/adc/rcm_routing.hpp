#pragma once

#include "dc24h/adc/rcm.hpp"
#include "dc24h/transfers/core.hpp"

namespace dc24h::adc {
enum class RcmRoutingError { invalid_state, wrong_source, invalid_message, recipient_unavailable,
    incompatible_connectivity, foreign_plan, missing_sink, already_dispatched };
namespace detail { struct RcmDomain final {}; }
class RcmFrame final {
public:
    RcmFrame(const RcmFrame&) = delete;
    RcmFrame& operator=(const RcmFrame&) = delete;
    RcmFrame(RcmFrame&&) = delete;
    RcmFrame& operator=(RcmFrame&&) = delete;
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
private:
    friend class RcmRouter;
    RcmFrame(Message message, std::string wire) : message_(std::move(message)), wire_(std::move(wire)) {}
    Message message_;
    std::string wire_;
};
using SharedRcmFrame = std::shared_ptr<const RcmFrame>;
class RoutedRcm final {
public:
    [[nodiscard]] const transfers::Plan& plan() const noexcept { return plan_; }
    [[nodiscard]] const SharedRcmFrame& frame() const noexcept { return frame_; }
private:
    friend class RcmRouter;
    RoutedRcm(std::shared_ptr<const detail::RcmDomain> domain, transfers::Plan plan, SharedRcmFrame frame)
        : domain_(std::move(domain)), plan_(std::move(plan)), frame_(std::move(frame)) {}
    std::shared_ptr<const detail::RcmDomain> domain_;
    transfers::Plan plan_;
    SharedRcmFrame frame_;
};
struct RcmDispatchReport {
    std::size_t queued{}, refused{}, canceled{};
    friend bool operator==(const RcmDispatchReport&, const RcmDispatchReport&) = default;
};
// Single owner-thread reverse coordination. Select a family with passive source
// and active target. No port fabrication, RCM->CTM synthesis, registry or I/O.
class RcmRouter final {
public:
    using Sink = std::function<bool(users::SessionId, const SharedRcmFrame&)>;
    explicit RcmRouter(transfers::Core& core) : core_(core) {}
    RcmRouter(const RcmRouter&) = delete;
    RcmRouter& operator=(const RcmRouter&) = delete;
    [[nodiscard]] std::variant<RoutedRcm, RcmRoutingError> prepare(users::SessionId sender, const Message& message);
    // Copies share one attempt. Foreign/missing-sink errors do not consume it.
    // Dispatch allocates nothing after sink creation; queue acceptance is not
    // connection/transfer success. E echo is an independent recipient copy.
    [[nodiscard]] std::variant<RcmDispatchReport, RcmRoutingError> dispatch(const RoutedRcm& message, const Sink& sink);
    [[nodiscard]] bool cancel(const RoutedRcm& message) noexcept;
private:
    transfers::Core& core_;
    const std::shared_ptr<const detail::RcmDomain> domain_{std::make_shared<const detail::RcmDomain>()};
};
} // namespace dc24h::adc
