#pragma once

#include "dc24h/transfers/request.hpp"

namespace dc24h::transfers {
enum class TcpMode { passive, active };
// Owning, fixed-size snapshot of public INF declarations. No reachability,
// authentication, NAT traversal or hub-transport inference is performed.
struct ClientCapabilities final {
    std::optional<users::AddressBytes> tcp4, tcp6;
    Protocols protocols;
    [[nodiscard]] std::optional<users::AddressBytes> listener(AddressFamily family) const noexcept;
    [[nodiscard]] TcpMode mode(AddressFamily family) const noexcept {
        return listener(family) ? TcpMode::active : TcpMode::passive;
    }
};
struct ConnectionChoice final {
    Kind kind;
    AddressFamily family;
    users::AddressBytes listener_address;
    friend bool operator==(const ConnectionChoice&, const ConnectionChoice&) = default;
};
[[nodiscard]] ClientCapabilities client_capabilities(const users::Profile& profile) noexcept;
// Validate a specific command/family without changing client intent.
[[nodiscard]] std::optional<users::AddressBytes> connection_address(
    const ClientCapabilities& origin, const ClientCapabilities& peer, Kind kind, AddressFamily family) noexcept;
// Choose IPv4, then IPv6 for an already selected command. This is admission
// metadata: CTM/RCM carry no family field and clients choose their own sockets.
[[nodiscard]] std::optional<ConnectionChoice> select_connection(
    const ClientCapabilities& origin, const ClientCapabilities& peer, Kind kind) noexcept;
// For trusted originators only: prefer CTM on either active family, then RCM
// on a passive-origin/active-peer family. Passive/passive has no direct path.
[[nodiscard]] std::optional<ConnectionChoice> select_connection(
    const ClientCapabilities& origin, const ClientCapabilities& peer) noexcept;
} // namespace dc24h::transfers
