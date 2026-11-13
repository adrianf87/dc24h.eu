#include "dc24h/transfers/security.hpp"
#include "dc24h/transfers/request.hpp"

namespace dc24h::transfers {
std::optional<PermissionError> authorize(const SecuritySettings& settings, Permissions sender, Permissions target, Kind kind) noexcept {
    if (kind != Kind::connect_to_me && kind != Kind::reverse_connect) { return PermissionError::forbidden; }
    if (kind == Kind::connect_to_me ? !settings.ctm_enabled : !settings.rcm_enabled) { return PermissionError::disabled; }
    if (!sender.initiate || !target.receive) { return PermissionError::forbidden; }
    return {};
}
} // namespace dc24h::transfers
