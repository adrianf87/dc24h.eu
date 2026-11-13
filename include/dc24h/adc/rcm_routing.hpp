#pragma once

#include "dc24h/adc/rcm.hpp"
#include "dc24h/adc/connection_routing.hpp"

namespace dc24h::adc {
// Command-specific compatibility facade over the common routing engine.
using RcmRoutingError = ConnectionRoutingError;
using RcmFrame = ConnectionFrame;
using SharedRcmFrame = SharedConnectionFrame;
using RoutedRcm = RoutedConnection;
using RcmDispatchReport = ConnectionDispatchReport;
class RcmRouter final {
public:
    using Sink = ConnectionRouter::Sink;
    explicit RcmRouter(transfers::Core& core) : router_(core, Command::rcm) {}
    RcmRouter(const RcmRouter&) = delete;
    RcmRouter& operator=(const RcmRouter&) = delete;
    [[nodiscard]] std::variant<RoutedRcm, RcmRoutingError> prepare(users::SessionId sender, const Message& message);
    [[nodiscard]] std::variant<RcmDispatchReport, RcmRoutingError> dispatch(const RoutedRcm& message, const Sink& sink);
    [[nodiscard]] bool cancel(const RoutedRcm& message) noexcept;
private:
    ConnectionRouter router_;
};
} // namespace dc24h::adc
