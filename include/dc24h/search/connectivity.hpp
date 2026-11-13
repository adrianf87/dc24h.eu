#pragma once

#include "dc24h/users/user.hpp"
#include <array>

namespace dc24h::search {
enum class AddressFamily { ipv4, ipv6 };
enum class ReplyRoute { hub, udp4, udp6 };
struct UdpEndpoint {
    AddressFamily family;
    std::array<unsigned char,16> address; // Network order; unused IPv4 bytes are zero.
    std::uint16_t port;
    friend bool operator==(const UdpEndpoint&, const UdpEndpoint&) = default;
};
// TCP and UDP activity are independent per family. An endpoint declaration is
// not a reachability test or permission to send packets. DRES remains available.
struct ReplyPaths {
    bool tcp4{}, tcp6{};
    std::optional<UdpEndpoint> udp4, udp6;
    [[nodiscard]] ReplyRoute route(AddressFamily family) const noexcept {
        if (family == AddressFamily::ipv4 && udp4) { return ReplyRoute::udp4; }
        if (family == AddressFamily::ipv6 && udp6) { return ReplyRoute::udp6; }
        return ReplyRoute::hub;
    }
    friend bool operator==(const ReplyPaths&, const ReplyPaths&) = default;
};
// Fixed-size owning metadata; no allocation, clocks, socket I/O or DNS.
// Requires the exact SU feature plus a usable numeric address; UDP additionally
// requires port 1..65535. The ADC adapter establishes peer-address ownership.
[[nodiscard]] ReplyPaths reply_paths(const users::Profile& profile) noexcept;
} // namespace dc24h::search
