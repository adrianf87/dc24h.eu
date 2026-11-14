#include "dc24h/transfers/protocol_policy.hpp"
#include <algorithm>
#include <stdexcept>

namespace dc24h::transfers {
namespace {
std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) { value.remove_prefix(1); }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) { value.remove_suffix(1); }
    return value;
}
}
ProtocolPolicy ProtocolPolicy::parse(std::string_view csv) {
    csv = trim(csv);
    if (csv == "any") { return {}; }
    ProtocolPolicy result;
    result.any_ = false;
    if (csv == "none") { return result; }
    for (;;) {
        const auto comma = csv.find(',');
        const auto name = trim(csv.substr(0, comma));
        if (!valid_wire_protocol(name) || name == "any" || name == "none" ||
            result.size_ == max_allowed_protocols ||
            std::find(result.names().begin(), result.names().end(), name) != result.names().end()) {
            throw std::invalid_argument("Invalid transfer allowed_protocols list");
        }
        result.names_[result.size_++] = name;
        if (comma == std::string_view::npos) { break; }
        csv.remove_prefix(comma + 1);
    }
    return result;
}
bool ProtocolPolicy::allows(std::string_view name) const noexcept {
    return any_ || std::find(names().begin(), names().end(), name) != names().end();
}
} // namespace dc24h::transfers
