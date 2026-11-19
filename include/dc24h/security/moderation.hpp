#pragma once
#include "dc24h/security/roles.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace dc24h::security {
// Allocation-free permission and target decision shared by kick, mute and bans.
// Readiness, time, password proof, schema and admission stay with the caller.
// A refusal is not a client message, an audit row or a permission grant.
enum class ModerationOperation : std::uint8_t {
    kick, mute, unmute, ban_account, ban_cid, ban_address, revoke_ban
};
enum class ModerationSubject : std::uint8_t { session, account, cid, address, ban_record };
enum class ModerationTargetRule : std::uint8_t {
    lower_role, lower_account_or_unmapped, permission_only, creator_or_lower
};
enum class ModerationRefusal : std::uint8_t {
    none, missing_permission, self_target, not_lower, wrong_subject
};
inline constexpr std::array<ModerationOperation, 7> moderation_catalog{
    ModerationOperation::kick, ModerationOperation::mute, ModerationOperation::unmute,
    ModerationOperation::ban_account, ModerationOperation::ban_cid,
    ModerationOperation::ban_address, ModerationOperation::revoke_ban};

struct ModerationRule {
    ModerationOperation operation{};
    Permission permission{};
    ModerationSubject subject{};
    ModerationTargetRule target{};
    bool guest_target{false};
    bool forbid_self{false};
    bool vip_delegable{false};
    bool requires_blocks{false};
};
// role is the session or account under the operation. For revocation of another
// actor's record it is the creator. account_bound is set for an account subject
// or a CID that already maps to an account. actor_is_creator skips the second
// rank comparison when revoking one's own record.
struct ModerationActor { accounts::Role role{accounts::Role::guest}; };
struct ModerationTarget {
    ModerationSubject subject{ModerationSubject::session};
    accounts::Role role{accounts::Role::guest};
    bool same_actor{false};
    bool account_bound{false};
    bool actor_is_creator{false};
};
struct ModerationDecision {
    ModerationRefusal refusal{ModerationRefusal::missing_permission};
    Permission permission{};
};

[[nodiscard]] std::string_view moderation_operation_key(ModerationOperation operation) noexcept;
[[nodiscard]] std::optional<ModerationOperation> parse_moderation_operation(std::string_view key) noexcept;
[[nodiscard]] std::string_view moderation_subject_key(ModerationSubject subject) noexcept;
[[nodiscard]] std::string_view moderation_target_key(ModerationTargetRule target) noexcept;
[[nodiscard]] std::string_view moderation_refusal_key(ModerationRefusal refusal) noexcept;
[[nodiscard]] ModerationRule moderation_rule(ModerationOperation operation) noexcept;
[[nodiscard]] bool actor_may(ModerationOperation operation, accounts::Role role, RolePolicy policy = {}) noexcept;
[[nodiscard]] ModerationDecision moderate(ModerationOperation operation, ModerationActor actor, ModerationTarget target, RolePolicy policy = {}) noexcept;
}
