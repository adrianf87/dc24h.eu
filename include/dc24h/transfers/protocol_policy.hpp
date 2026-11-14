#pragma once

#include "dc24h/transfers/protocol.hpp"
#include <array>
#include <span>

namespace dc24h::transfers {
inline constexpr std::size_t max_allowed_protocols = 16;
// Startup parsing owns exact names. Admission/dispatch lookups allocate nothing
// and never negotiate a fallback. Only factories can change the policy.
class ProtocolPolicy final {
public:
    ProtocolPolicy() = default;
    [[nodiscard]] static ProtocolPolicy parse(std::string_view csv);
    [[nodiscard]] bool any() const noexcept { return any_; }
    [[nodiscard]] std::span<const std::string> names() const noexcept { return {names_.data(), size_}; }
    [[nodiscard]] bool allows(std::string_view name) const noexcept;
private:
    bool any_{true};
    std::array<std::string, max_allowed_protocols> names_{};
    std::size_t size_{};
};
} // namespace dc24h::transfers
