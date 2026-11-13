#include "dc24h/search/connectivity.hpp"
#include <algorithm>
#include <arpa/inet.h>

namespace dc24h::search {
namespace {
std::optional<std::array<unsigned char,16>> address(const std::string& text, AddressFamily family) noexcept {
    std::array<unsigned char,16> bytes{};
    if (text.empty() || inet_pton(family == AddressFamily::ipv4 ? AF_INET : AF_INET6,text.c_str(),bytes.data()) != 1) { return {}; }
    const auto length = family == AddressFamily::ipv4 ? 4 : 16;
    if (std::all_of(bytes.begin(),bytes.begin()+length,[](auto b){return b == 0;})) { return {}; }
    if (family == AddressFamily::ipv4) {
        // No multicast or limited broadcast destinations from generic profiles.
        if ((bytes[0] & 0xf0) == 0xe0 || std::all_of(bytes.begin(),bytes.begin()+4,[](auto b){return b == 255;})) { return {}; }
    } else {
        if (bytes[0] == 255) { return {}; } // Multicast; scope IDs are not supported.
        const std::array<unsigned char,12> mapped{0,0,0,0,0,0,0,0,0,0,255,255};
        if (std::equal(mapped.begin(),mapped.end(),bytes.begin())) { return {}; }
        if (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80) { return {}; } // Link-local requires a zone.
    }
    return bytes;
}
} // namespace
ReplyPaths reply_paths(const users::Profile& profile) noexcept {
    const auto& data = profile.data();
    const auto has = [&](std::string_view name) { return std::find(data.features.begin(),data.features.end(),name) != data.features.end(); };
    const auto v4 = address(data.ipv4,AddressFamily::ipv4), v6 = address(data.ipv6,AddressFamily::ipv6);
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
