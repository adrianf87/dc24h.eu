#pragma once
#include "dc24h/users/session_manager.hpp"
#include "dc24h/security/roles.hpp"
#include <array>
#include <string_view>

namespace dc24h::admin {
enum class MuteScope { public_chat, private_chat, all };
enum class MuteError { none, forbidden, unavailable, invalid_time, invalid_scope, resources };
[[nodiscard]] std::string_view mute_scope_key(MuteScope scope) noexcept;
[[nodiscard]] std::optional<MuteScope> parse_mute_scope(std::string_view key) noexcept;
inline constexpr std::array<std::string_view,8> mute_statistic_names{
    "attempts","applied","cleared","refused","errors","notice_failures","public_blocked","private_blocked"};
struct MuteStatistics {std::array<std::uint64_t,mute_statistic_names.size()> values{};};
// Live session policy; no SQL, secrets, allocation, clock reads or history.
// A ready authenticated actor may change only a strictly lower ready session.
class MuteCore final {
public:
    MuteCore(users::SessionManager& sessions,security::RolePolicy policy) noexcept:sessions_(sessions),policy_(policy){}
    [[nodiscard]] MuteError check(users::SessionId actor,users::SessionId target,MuteScope scope,users::TimePoint now) const noexcept;
    [[nodiscard]] MuteError apply(users::SessionId actor,users::SessionId target,MuteScope scope,bool enabled,users::TimePoint now) noexcept;
private:
    users::SessionManager& sessions_;
    security::RolePolicy policy_;
};
}
