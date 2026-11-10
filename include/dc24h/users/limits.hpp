#pragma once

#include <cstddef>

namespace dc24h::users {
// Positive ceilings, shared by ADC and ADCS. Defaults preserve the existing
// network cap; a larger user ceiling never raises the session/connection cap.
struct Limits {
    static constexpr std::size_t max_capacity = 65535;
    std::size_t max_sessions{max_capacity};
    std::size_t max_users{max_capacity};
    [[nodiscard]] Limits bounded_by(std::size_t max_connections) const;
    friend bool operator==(const Limits&, const Limits&) = default;
};
void validate(const Limits& limits);
} // namespace dc24h::users
