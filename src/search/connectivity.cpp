#include "dc24h/search/connectivity.hpp"
#include <algorithm>

namespace dc24h::search {
ReplyPaths reply_paths(const users::Profile& profile) noexcept {
    const auto& data = profile.data();
    const auto has = [&](std::string_view name) { return std::find(data.features.begin(),data.features.end(),name) != data.features.end(); };
    const auto v4 = users::public_address(profile,AddressFamily::ipv4), v6 = users::public_address(profile,AddressFamily::ipv6);
    ReplyPaths paths;
    paths.tcp4 = v4 && has("TCP4"); paths.tcp6 = v6 && has("TCP6");
    if (v4 && has("UDP4") && data.udp4_port && *data.udp4_port > 0 && *data.udp4_port <= 65535) {
        paths.udp4 = UdpEndpoint{AddressFamily::ipv4,*v4,static_cast<std::uint16_t>(*data.udp4_port)};
    }
    if (v6 && has("UDP6") && data.udp6_port && *data.udp6_port > 0 && *data.udp6_port <= 65535) {
        paths.udp6 = UdpEndpoint{AddressFamily::ipv6,*v6,static_cast<std::uint16_t>(*data.udp6_port)};
    }
    return paths;
}
} // namespace dc24h::search
