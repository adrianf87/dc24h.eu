#include "dc24h/adc/connection_errors.hpp"

namespace dc24h::adc {
ConnectionFailure connection_failure(ConnectionRoutingError error, Command command) noexcept {
    if (command != Command::ctm && command != Command::rcm) { return {FailureAction::abort, {}, {}}; }
    using enum ConnectionRoutingError;
    switch (error) {
    case invalid_state: return {FailureAction::terminate,"244","Invalid connection state"};
    case wrong_source: return {FailureAction::terminate,"240","Protocol error"};
    case recipient_unavailable: return {FailureAction::reply,"140",command == Command::ctm ? "CTM recipient unavailable" : "RCM recipient unavailable"};
    case inactive_sender:
        if (command != Command::ctm) { return {FailureAction::abort, {}, {}}; }
        return {FailureAction::reply,"100","CTM sender is not TCP active"};
    case incompatible_connectivity:
        if (command != Command::rcm) { return {FailureAction::abort, {}, {}}; }
        return {FailureAction::reply,"100","RCM requires passive sender and active recipient"};
    case invalid_message: return {FailureAction::reply,"100","Invalid connection request"};
    case disabled: return {FailureAction::reply,"125","Connection operation is disabled"};
    case forbidden: return {FailureAction::reply,"125","Connection operation not permitted"};
    default: return {FailureAction::abort, {}, {}};
    }
}
} // namespace dc24h::adc
