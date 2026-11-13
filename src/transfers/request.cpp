#include "dc24h/transfers/request.hpp"
#include "dc24h/text/public_text.hpp"

namespace dc24h::transfers {
Request::Request(const RequestData& data, Protocol protocol)
    : kind_(data.kind), family_(data.family), protocol_(std::move(protocol)), token_(data.token),
      port_(data.port ? std::optional{static_cast<std::uint16_t>(*data.port)} : std::nullopt) {}
std::variant<Request, RequestError> Request::create(const RequestData& data) {
    if (data.kind != Kind::connect_to_me && data.kind != Kind::reverse_connect) { return RequestError::invalid_kind; }
    if (data.family != AddressFamily::ipv4 && data.family != AddressFamily::ipv6) { return RequestError::invalid_family; }
    if ((data.kind == Kind::connect_to_me && (!data.port || *data.port < 1 || *data.port > 65535)) ||
        (data.kind == Kind::reverse_connect && data.port)) { return RequestError::invalid_port; }
    // Enforce byte ceilings before normalization or owning copies.
    if (data.protocol.empty() || data.protocol.size() > max_protocol_bytes) { return RequestError::invalid_protocol; }
    if (data.token.empty() || data.token.size() > max_token_bytes) { return RequestError::invalid_token; }
    if (!text::valid_public_text(data.protocol)) { return RequestError::invalid_protocol; }
    if (!text::valid_public_text(data.token)) { return RequestError::invalid_token; }
    auto protocol = Protocol::create(data.protocol);
    if (!protocol) { return RequestError::invalid_protocol; }
    return Request(data, std::move(*protocol));
}
} // namespace dc24h::transfers
