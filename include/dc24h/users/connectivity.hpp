#pragma once

#include "dc24h/users/user.hpp"
#include <array>

namespace dc24h::users {
enum class AddressFamily { ipv4, ipv6 };
using AddressBytes = std::array<unsigned char, 16>;
// Canonical binary listener address: no unspecified/multicast/limited broadcast,
// mapped IPv6 or scope-dependent IPv6 link-local; IPv4 padding must be zero.
// LAN/loopback addresses remain valid; this is not a reachability test.
[[nodiscard]] bool usable_address(const AddressBytes& address, AddressFamily family) noexcept;
// Fixed-size owning public numeric address, with zero IPv4 padding. No DNS,
// I/O, allocation or reachability claim. The ADC adapter verifies peer ownership.
[[nodiscard]] std::optional<AddressBytes> public_address(const Profile& profile, AddressFamily family) noexcept;
[[nodiscard]] bool tcp_active(const Profile& profile, AddressFamily family) noexcept;
} // namespace dc24h::users
