#pragma once

#include "dc24h/users/connectivity.hpp"
#include "dc24h/transfers/protocol.hpp"
#include "dc24h/transfers/token.hpp"
#include <variant>

namespace dc24h::transfers {
using AddressFamily = users::AddressFamily;
enum class Kind { connect_to_me, reverse_connect };
enum class RequestError { invalid_kind, invalid_family, invalid_protocol, invalid_port, invalid_token };
struct RequestData {
    Kind kind{Kind::connect_to_me};
    AddressFamily family{AddressFamily::ipv4};
    std::string protocol{"ADC/1.0"};
    std::optional<std::int64_t> port;
    std::string token;
};
// Validated, bounded and owning coordination intent. Protocol records known
// encryption requirements while retaining extensions; the token is opaque.
// Neither is authentication or a verified claim of peer protocol/TLS support.
class Request final {
public:
    [[nodiscard]] static std::variant<Request, RequestError> create(const RequestData& data);
    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    [[nodiscard]] AddressFamily family() const noexcept { return family_; }
    [[nodiscard]] const std::string& protocol() const noexcept { return protocol_.name(); }
    [[nodiscard]] const Protocol& protocol_value() const noexcept { return protocol_; }
    [[nodiscard]] std::optional<std::uint16_t> port() const noexcept { return port_; }
    [[nodiscard]] const std::string& token() const noexcept { return token_.value(); }
    [[nodiscard]] const Token& token_value() const noexcept { return token_; }
private:
    Request(const RequestData& data, Protocol protocol, Token token);
    Kind kind_;
    AddressFamily family_;
    Protocol protocol_;
    Token token_;
    std::optional<std::uint16_t> port_;
};
} // namespace dc24h::transfers
