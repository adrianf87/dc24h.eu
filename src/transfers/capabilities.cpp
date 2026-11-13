#include "dc24h/transfers/capabilities.hpp"

namespace dc24h::transfers {
std::optional<users::AddressBytes> ClientCapabilities::listener(AddressFamily family) const noexcept {
    if (family == AddressFamily::ipv4) { return tcp4; }
    if (family == AddressFamily::ipv6) { return tcp6; }
    return {};
}
ClientCapabilities client_capabilities(const users::Profile& profile) noexcept {
    ClientCapabilities result{{}, {}, advertised_protocols(profile)};
    if (users::tcp_active(profile, AddressFamily::ipv4)) { result.tcp4 = users::public_address(profile, AddressFamily::ipv4); }
    if (users::tcp_active(profile, AddressFamily::ipv6)) { result.tcp6 = users::public_address(profile, AddressFamily::ipv6); }
    return result;
}
std::optional<users::AddressBytes> connection_address(const ClientCapabilities& origin,
    const ClientCapabilities& peer, Kind kind, AddressFamily family) noexcept {
    if (kind == Kind::connect_to_me) { return origin.listener(family); }
    if (kind == Kind::reverse_connect && !origin.listener(family)) { return peer.listener(family); }
    return {};
}
std::optional<ConnectionChoice> select_connection(const ClientCapabilities& origin,
    const ClientCapabilities& peer, Kind kind) noexcept {
    for (auto family : {AddressFamily::ipv4, AddressFamily::ipv6}) {
        if (const auto address = connection_address(origin, peer, kind, family)) {
            return ConnectionChoice{kind, family, *address};
        }
    }
    return {};
}
std::optional<ConnectionChoice> select_connection(const ClientCapabilities& origin, const ClientCapabilities& peer) noexcept {
    if (auto choice = select_connection(origin, peer, Kind::connect_to_me)) { return choice; }
    return select_connection(origin, peer, Kind::reverse_connect);
}
} // namespace dc24h::transfers
