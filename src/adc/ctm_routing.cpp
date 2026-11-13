#include "dc24h/adc/ctm_routing.hpp"

namespace dc24h::adc {
std::variant<RoutedCtm, CtmRoutingError> CtmRouter::prepare(users::SessionId sender, const Message& message) { return router_.prepare(sender, message); }
std::variant<CtmDispatchReport, CtmRoutingError> CtmRouter::dispatch(const RoutedCtm& message, const Sink& sink) { return router_.dispatch(message, sink); }
bool CtmRouter::cancel(const RoutedCtm& message) noexcept { return router_.cancel(message); }
} // namespace dc24h::adc
