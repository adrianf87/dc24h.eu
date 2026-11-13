#include "dc24h/transfers/protocol.hpp"
#include "dc24h/text/public_text.hpp"
#include "dc24h/users/user.hpp"
#include <algorithm>

namespace dc24h::transfers {
std::optional<Protocol> Protocol::create(std::string_view name) {
    if (name.empty() || name.size() > max_protocol_bytes || !text::valid_public_text(name)) { return {}; }
    return Protocol(name);
}
Protocols advertised_protocols(const users::Profile& profile) noexcept {
    const auto& features = profile.data().features;
    return {true, std::find(features.begin(), features.end(), "ADCS") != features.end()};
}
std::variant<ProtocolKind, NegotiationError> negotiate_protocol(Protocols origin, Protocols peer, ProtocolPreference preference) noexcept {
    if (preference != ProtocolPreference::prefer_adc && preference != ProtocolPreference::prefer_adcs &&
        preference != ProtocolPreference::require_adcs) { return NegotiationError::invalid_preference; }
    const bool adc = origin.adc && peer.adc, adcs = origin.adcs && peer.adcs;
    if (preference == ProtocolPreference::require_adcs) {
        if (adcs) { return ProtocolKind::adcs_1_0; }
        return NegotiationError::encryption_unavailable;
    }
    if (preference == ProtocolPreference::prefer_adc && adc) { return ProtocolKind::adc_1_0; }
    if (adcs) { return ProtocolKind::adcs_1_0; }
    if (adc) { return ProtocolKind::adc_1_0; }
    return NegotiationError::no_common_protocol;
}
} // namespace dc24h::transfers
