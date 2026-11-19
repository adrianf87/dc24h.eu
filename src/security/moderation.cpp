#include "dc24h/security/moderation.hpp"

namespace dc24h::security {
namespace {
bool strictly_lower(accounts::Role actor, accounts::Role target) noexcept {
    const auto left = role_rank(actor), right = role_rank(target);
    return left && right && *left > *right;
}
bool concrete_ban(ModerationSubject subject) noexcept {
    return subject == ModerationSubject::account || subject == ModerationSubject::cid || subject == ModerationSubject::address;
}
}

std::string_view moderation_operation_key(ModerationOperation operation) noexcept {
    switch (operation) {
    case ModerationOperation::kick: return "kick";
    case ModerationOperation::mute: return "mute";
    case ModerationOperation::unmute: return "unmute";
    case ModerationOperation::ban_account: return "ban_account";
    case ModerationOperation::ban_cid: return "ban_cid";
    case ModerationOperation::ban_address: return "ban_address";
    case ModerationOperation::revoke_ban: return "revoke_ban";
    }
    return {};
}
std::optional<ModerationOperation> parse_moderation_operation(std::string_view key) noexcept {
    for (const auto operation : moderation_catalog) {
        if (moderation_operation_key(operation) == key) { return operation; }
    }
    return std::nullopt;
}
std::string_view moderation_subject_key(ModerationSubject subject) noexcept {
    switch (subject) {
    case ModerationSubject::session: return "session";
    case ModerationSubject::account: return "account";
    case ModerationSubject::cid: return "cid";
    case ModerationSubject::address: return "address";
    case ModerationSubject::ban_record: return "ban_record";
    }
    return {};
}
std::string_view moderation_target_key(ModerationTargetRule target) noexcept {
    switch (target) {
    case ModerationTargetRule::lower_role: return "lower_role";
    case ModerationTargetRule::lower_account_or_unmapped: return "lower_account_or_unmapped";
    case ModerationTargetRule::permission_only: return "permission_only";
    case ModerationTargetRule::creator_or_lower: return "creator_or_lower";
    }
    return {};
}
std::string_view moderation_refusal_key(ModerationRefusal refusal) noexcept {
    switch (refusal) {
    case ModerationRefusal::none: return "none";
    case ModerationRefusal::missing_permission: return "missing_permission";
    case ModerationRefusal::self_target: return "self_target";
    case ModerationRefusal::not_lower: return "not_lower";
    case ModerationRefusal::wrong_subject: return "wrong_subject";
    }
    return {};
}
ModerationRule moderation_rule(ModerationOperation operation) noexcept {
    using Op = ModerationOperation;
    using Subject = ModerationSubject;
    using Target = ModerationTargetRule;
    switch (operation) {
    case Op::kick:
        return {operation, Permission::revoke_sessions, Subject::session, Target::lower_role, true, true, true, false};
    case Op::mute:
    case Op::unmute:
        return {operation, Permission::mute_users, Subject::session, Target::lower_role, true, true, true, false};
    case Op::ban_account:
        return {operation, Permission::ban_users, Subject::account, Target::lower_account_or_unmapped, false, true, false, true};
    case Op::ban_cid:
        return {operation, Permission::ban_users, Subject::cid, Target::lower_account_or_unmapped, false, true, false, true};
    case Op::ban_address:
        return {operation, Permission::ban_users, Subject::address, Target::permission_only, false, false, false, true};
    case Op::revoke_ban:
        return {operation, Permission::ban_users, Subject::ban_record, Target::creator_or_lower, false, false, false, true};
    }
    return {};
}
bool actor_may(ModerationOperation operation, accounts::Role role, RolePolicy policy) noexcept {
    if (moderation_operation_key(operation).empty()) { return false; }
    const auto rule = moderation_rule(operation);
    if (permission_key(rule.permission).empty() || !allows(role, rule.permission, policy)) { return false; }
    return !rule.requires_blocks || allows(role, Permission::manage_blocks, policy);
}
ModerationDecision moderate(ModerationOperation operation, ModerationActor actor, ModerationTarget target, RolePolicy policy) noexcept {
    const auto rule = moderation_rule(operation);
    ModerationDecision decision{ModerationRefusal::missing_permission, rule.permission};
    if (!actor_may(operation, actor.role, policy)) { return decision; }
    const auto record = rule.target == ModerationTargetRule::creator_or_lower;
    if (record ? !concrete_ban(target.subject) : target.subject != rule.subject) {
        decision.refusal = ModerationRefusal::wrong_subject;
        return decision;
    }
    switch (rule.target) {
    case ModerationTargetRule::lower_role:
        if (rule.forbid_self && target.same_actor) { decision.refusal = ModerationRefusal::self_target; return decision; }
        if (!strictly_lower(actor.role, target.role)) { decision.refusal = ModerationRefusal::not_lower; return decision; }
        break;
    case ModerationTargetRule::lower_account_or_unmapped:
        if (operation == ModerationOperation::ban_cid && !target.account_bound) { break; }
        if (!target.account_bound || !accounts::account_role(target.role)) {
            decision.refusal = ModerationRefusal::wrong_subject;
            return decision;
        }
        if (rule.forbid_self && target.same_actor) { decision.refusal = ModerationRefusal::self_target; return decision; }
        if (!strictly_lower(actor.role, target.role)) { decision.refusal = ModerationRefusal::not_lower; return decision; }
        break;
    case ModerationTargetRule::permission_only:
        break;
    case ModerationTargetRule::creator_or_lower:
        if (target.actor_is_creator) { break; }
        if (!accounts::account_role(target.role) || !strictly_lower(actor.role, target.role)) {
            decision.refusal = ModerationRefusal::not_lower;
            return decision;
        }
        break;
    }
    decision.refusal = ModerationRefusal::none;
    return decision;
}
}
