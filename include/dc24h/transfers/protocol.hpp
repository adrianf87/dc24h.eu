#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace dc24h::users { class Profile; }
namespace dc24h::transfers {
inline constexpr std::size_t max_protocol_bytes = 32;
enum class ProtocolKind { adc_1_0, adcs_1_0, extension };
enum class Encryption { plaintext, tls_required, peer_defined };
[[nodiscard]] constexpr ProtocolKind classify_protocol(std::string_view name) noexcept {
    return name == "ADC/1.0" ? ProtocolKind::adc_1_0 :
        name == "ADCS/1.0" ? ProtocolKind::adcs_1_0 : ProtocolKind::extension;
}
[[nodiscard]] constexpr Encryption encryption(ProtocolKind kind) noexcept {
    return kind == ProtocolKind::adc_1_0 ? Encryption::plaintext :
        kind == ProtocolKind::adcs_1_0 ? Encryption::tls_required : Encryption::peer_defined;
}
[[nodiscard]] constexpr std::optional<std::string_view> protocol_name(ProtocolKind kind) noexcept {
    if (kind == ProtocolKind::adc_1_0) { return "ADC/1.0"; }
    if (kind == ProtocolKind::adcs_1_0) { return "ADCS/1.0"; }
    return {};
}
// Owning, checked protocol intent. Unknown extensions keep their exact name;
// peer_defined must never be interpreted as permission to use plaintext.
class Protocol final {
public:
    [[nodiscard]] static std::optional<Protocol> create(std::string_view name);
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] ProtocolKind kind() const noexcept { return classify_protocol(name_); }
    [[nodiscard]] Encryption encryption() const noexcept { return transfers::encryption(kind()); }
    friend bool operator==(const Protocol&, const Protocol&) = default;
private:
    explicit Protocol(std::string_view name) : name_(name) {}
    std::string name_;
};
struct Protocols {
    bool adc{true};
    bool adcs{false};
    friend bool operator==(const Protocols&, const Protocols&) = default;
};
enum class ProtocolPreference { prefer_adc, prefer_adcs, require_adcs };
enum class NegotiationError { invalid_preference, no_common_protocol, encryption_unavailable };
// Exact public SU ADCS declaration, not SUP, TCP activity, hub transport or a
// verified TLS capability. ADC0 alone does not advertise modern ADCS/1.0.
[[nodiscard]] Protocols advertised_protocols(const users::Profile& profile) noexcept;
// Pure selection for a trusted request originator. Forwarding an existing
// CTM/RCM must preserve its explicit protocol instead of running selection.
// require_adcs can never fall back to ADC. No allocations or state changes.
[[nodiscard]] std::variant<ProtocolKind, NegotiationError> negotiate_protocol(
    Protocols origin, Protocols peer, ProtocolPreference preference) noexcept;
} // namespace dc24h::transfers
