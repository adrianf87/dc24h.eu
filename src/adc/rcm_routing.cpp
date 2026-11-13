#include "dc24h/adc/rcm_routing.hpp"

namespace dc24h::adc {
std::variant<RoutedRcm, RcmRoutingError> RcmRouter::prepare(users::SessionId sender, const Message& message) { return router_.prepare(sender, message); }
std::variant<RcmDispatchReport, RcmRoutingError> RcmRouter::dispatch(const RoutedRcm& message, const Sink& sink) { return router_.dispatch(message, sink); }
bool RcmRouter::cancel(const RoutedRcm& message) noexcept { return router_.cancel(message); }
} // namespace dc24h::adc
