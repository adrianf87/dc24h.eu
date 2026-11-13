#pragma once

#include "dc24h/users/connectivity.hpp"
#include <variant>

namespace dc24h::transfers {
using AddressFamily = users::AddressFamily;
enum class Kind { connect_to_me, reverse_connect };
enum class RequestError { invalid_kind, invalid_family, invalid_protocol, invalid_port, invalid_token };
inline constexpr std::size_t max_protocol_bytes = 32;
inline constexpr std::size_t max_token_bytes = 128;
struct RequestData {
    Kind kind{Kind::connect_to_me};
    AddressFamily family{AddressFamily::ipv4};
    std::string protocol{"ADC/1.0"};
    std::optional<std::int64_t> port;
    std::string token;
};
// Validated, bounded and owning coordination intent. Protocol and token are
// opaque NFC text, not authentication or a claim of protocol/TLS support.
class Request final {
public:
    [[nodiscard]] static std::variant<Request, RequestError> create(const RequestData& data);
    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    [[nodiscard]] AddressFamily family() const noexcept { return family_; }
    [[nodiscard]] const std::string& protocol() const noexcept { return protocol_; }
    [[nodiscard]] std::optional<std::uint16_t> port() const noexcept { return port_; }
    [[nodiscard]] const std::string& token() const noexcept { return token_; }
private:
    explicit Request(const RequestData& data);
    Kind kind_;
    AddressFamily family_;
    std::string protocol_, token_;
    std::optional<std::uint16_t> port_;
};
} // namespace dc24h::transfers
