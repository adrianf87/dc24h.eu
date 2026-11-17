#pragma once
#include "dc24h/users/session_manager.hpp"
#include "dc24h/security/roles.hpp"
#include <array>
#include <string_view>

namespace dc24h::admin {
enum class KickError { none, forbidden, unavailable, invalid_time, invalid_reason, resources };
inline constexpr std::size_t max_kick_reason_bytes=256;
inline constexpr std::array<std::string_view,5> kick_statistic_names{"attempts","succeeded","refused","errors","notice_failures"};
struct KickStatistics { std::array<std::uint64_t,5> values{}; };
// Live reactor policy: actual transport actor and current lifetime target only.
// No protocol, SQL, proof, history, allocation or client-selected actor identity.
// Adapter prepares bounded notices first, then commits and clears routing state.
class KickCore final {
public:
    KickCore(users::SessionManager& sessions,security::RolePolicy policy) noexcept : sessions_(sessions),policy_(policy) {}
    [[nodiscard]] KickError check(users::SessionId actor,users::SessionId target,users::TimePoint now) const noexcept;
    [[nodiscard]] KickError apply(users::SessionId actor,users::SessionId target,users::TimePoint now) noexcept;
private:
    users::SessionManager& sessions_;
    security::RolePolicy policy_;
};
} // namespace dc24h::admin
