#pragma once

#include "dc24h/adc/ctm.hpp"
#include "dc24h/adc/connection_routing.hpp"

namespace dc24h::adc {
// Command-specific compatibility facade over the common routing engine.
using CtmRoutingError = ConnectionRoutingError;
using CtmFrame = ConnectionFrame;
using SharedCtmFrame = SharedConnectionFrame;
using RoutedCtm = RoutedConnection;
using CtmDispatchReport = ConnectionDispatchReport;
class CtmRouter final {
public:
    using Sink = ConnectionRouter::Sink;
    explicit CtmRouter(transfers::Core& core) : router_(core, Command::ctm) {}
    CtmRouter(const CtmRouter&) = delete;
    CtmRouter& operator=(const CtmRouter&) = delete;
    [[nodiscard]] std::variant<RoutedCtm, CtmRoutingError> prepare(users::SessionId sender, const Message& message);
    [[nodiscard]] std::variant<CtmDispatchReport, CtmRoutingError> dispatch(const RoutedCtm& message, const Sink& sink);
    [[nodiscard]] bool cancel(const RoutedCtm& message) noexcept;
private:
    ConnectionRouter router_;
};
} // namespace dc24h::adc
