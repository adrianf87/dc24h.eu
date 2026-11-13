#include "dc24h/users/connectivity.hpp"

#include <algorithm>
#include <arpa/inet.h>

namespace dc24h::users {
std::optional<AddressBytes> public_address(const Profile& profile, AddressFamily family) noexcept {
    if (family != AddressFamily::ipv4 && family != AddressFamily::ipv6) { return {}; }
    const auto& data = profile.data();
    const auto& text = family == AddressFamily::ipv4 ? data.ipv4 : data.ipv6;
    AddressBytes bytes{};
    if (text.empty() || inet_pton(family == AddressFamily::ipv4 ? AF_INET : AF_INET6, text.c_str(), bytes.data()) != 1) { return {}; }
    const auto length = family == AddressFamily::ipv4 ? 4 : 16;
    if (std::all_of(bytes.begin(), bytes.begin() + length, [](auto b) { return b == 0; })) { return {}; }
    if (family == AddressFamily::ipv4) {
        if ((bytes[0] & 0xf0) == 0xe0 || std::all_of(bytes.begin(), bytes.begin() + 4, [](auto b) { return b == 255; })) { return {}; }
    } else {
        if (bytes[0] == 255) { return {}; }
        const std::array<unsigned char, 12> mapped{0,0,0,0,0,0,0,0,0,0,255,255};
        if (std::equal(mapped.begin(), mapped.end(), bytes.begin())) { return {}; }
        if (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80) { return {}; }
    }
    return bytes;
}
bool tcp_active(const Profile& profile, AddressFamily family) noexcept {
    if (!public_address(profile, family)) { return false; }
    const auto& features = profile.data().features;
    const auto name = family == AddressFamily::ipv4 ? "TCP4" : "TCP6";
    return std::find(features.begin(), features.end(), name) != features.end();
}
} // namespace dc24h::users
