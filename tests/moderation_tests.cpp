#include "dc24h/security/moderation.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <source_location>
#include <stdexcept>

namespace { int fail_after = -1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_after == 0) { fail_after = -1; throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* value = std::malloc(size ? size : 1)) { return value; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* pointer) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete[](void* pointer) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace {
using namespace dc24h;
using accounts::Role;
using security::ModerationOperation;
using security::ModerationRefusal;
using security::ModerationSubject;
using security::ModerationTarget;
void require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("Moderation assertion at line " + std::to_string(where.line())); }
}
constexpr Role roles[]{Role::guest, Role::registered, Role::vip, Role::moderator, Role::operator_, Role::administrator, Role::owner};
security::ModerationDecision decide(ModerationOperation operation, Role actor, ModerationTarget target, security::RolePolicy policy = {}) {
    return security::moderate(operation, {actor}, target, policy);
}
ModerationTarget session(Role role, bool same = false) {
    ModerationTarget target;
    target.subject = ModerationSubject::session;
    target.role = role;
    target.same_actor = same;
    return target;
}
ModerationTarget account(Role role, bool same = false) {
    ModerationTarget target;
    target.subject = ModerationSubject::account;
    target.role = role;
    target.same_actor = same;
    target.account_bound = true;
    return target;
}
bool lower(Role actor, Role target) {
    const auto left = security::role_rank(actor), right = security::role_rank(target);
    return left && right && *left > *right;
}

void catalog() {
    for (const auto operation : security::moderation_catalog) {
        const auto key = security::moderation_operation_key(operation);
        require(security::parse_moderation_operation(key) == operation);
        const auto rule = security::moderation_rule(operation);
        require(rule.operation == operation);
        require(!security::moderation_subject_key(rule.subject).empty());
        require(!security::moderation_target_key(rule.target).empty());
        require(!security::permission_key(rule.permission).empty());
    }
    require(!security::parse_moderation_operation(""));
    require(!security::parse_moderation_operation("KICK"));
    require(!security::parse_moderation_operation("ban"));
    require(security::moderation_operation_key(static_cast<ModerationOperation>(255)).empty());
    require(!security::actor_may(static_cast<ModerationOperation>(255), Role::owner));
    for (const auto refusal : {ModerationRefusal::none, ModerationRefusal::missing_permission, ModerationRefusal::self_target, ModerationRefusal::not_lower, ModerationRefusal::wrong_subject}) {
        require(!security::moderation_refusal_key(refusal).empty());
    }
}

void sessions() {
    const auto kick = security::parse_vip_permissions("users.kick");
    const auto mute = security::parse_vip_permissions("users.mute");
    for (const auto actor : roles) {
        for (const auto target : roles) {
            const auto kicked = decide(ModerationOperation::kick, actor, session(target));
            const auto muted = decide(ModerationOperation::mute, actor, session(target));
            const auto unmuted = decide(ModerationOperation::unmute, actor, session(target));
            const bool staff = security::role_rank(actor) && *security::role_rank(actor) >= 3 && lower(actor, target);
            require((kicked.refusal == ModerationRefusal::none) == staff);
            require((muted.refusal == ModerationRefusal::none) == staff);
            require(muted.refusal == unmuted.refusal);
            require(security::can_revoke(actor, target) == (kicked.refusal == ModerationRefusal::none));
            require(security::can_mute(actor, target) == (muted.refusal == ModerationRefusal::none));
            const auto delegated_kick = decide(ModerationOperation::kick, actor, session(target), kick);
            const bool vip_kick = actor == Role::vip && lower(actor, target);
            require((delegated_kick.refusal == ModerationRefusal::none) == (staff || vip_kick));
            require(decide(ModerationOperation::mute, actor, session(target), kick).refusal != ModerationRefusal::none || actor != Role::vip);
            const auto delegated_mute = decide(ModerationOperation::mute, actor, session(target), mute);
            require((delegated_mute.refusal == ModerationRefusal::none) == (staff || (actor == Role::vip && lower(actor, target))));
            require(decide(ModerationOperation::kick, actor, session(target), mute).refusal != ModerationRefusal::none || actor != Role::vip);
        }
    }
    require(decide(ModerationOperation::kick, Role::owner, session(Role::guest, true)).refusal == ModerationRefusal::self_target);
    require(decide(ModerationOperation::mute, Role::moderator, session(Role::registered, true)).refusal == ModerationRefusal::self_target);
    require(decide(ModerationOperation::kick, Role::operator_, account(Role::registered)).refusal == ModerationRefusal::wrong_subject);
    require(decide(ModerationOperation::kick, Role::registered, session(Role::guest)).refusal == ModerationRefusal::missing_permission);
    require(decide(ModerationOperation::kick, Role::guest, session(Role::guest)).refusal == ModerationRefusal::missing_permission);
    security::RolePolicy invalid;
    invalid.vip_permissions = 0xffffffffu;
    require(decide(ModerationOperation::kick, Role::owner, session(Role::guest), invalid).refusal == ModerationRefusal::missing_permission);
}

void bans() {
    for (const auto actor : roles) {
        const bool staff = security::actor_may(ModerationOperation::ban_account, actor);
        require(staff == (security::role_rank(actor) && *security::role_rank(actor) >= 4));
        require(security::actor_may(ModerationOperation::ban_cid, actor) == staff);
        require(security::actor_may(ModerationOperation::ban_address, actor) == staff);
        require(security::actor_may(ModerationOperation::revoke_ban, actor) == staff);
        for (const auto target : roles) {
            const auto created = decide(ModerationOperation::ban_account, actor, account(target));
            const bool allowed = staff && target != Role::guest && lower(actor, target);
            require((created.refusal == ModerationRefusal::none) == allowed);
            if (staff && target != Role::guest && actor == target) {
                require(decide(ModerationOperation::ban_account, actor, account(target, true)).refusal == ModerationRefusal::self_target);
            }
            ModerationTarget mapped;
            mapped.subject = ModerationSubject::cid;
            mapped.role = target;
            mapped.account_bound = true;
            mapped.same_actor = actor == target;
            const auto cid = decide(ModerationOperation::ban_cid, actor, mapped);
            require((cid.refusal == ModerationRefusal::none) == allowed);
        }
        ModerationTarget unmapped;
        unmapped.subject = ModerationSubject::cid;
        require((decide(ModerationOperation::ban_cid, actor, unmapped).refusal == ModerationRefusal::none) == staff);
        ModerationTarget address;
        address.subject = ModerationSubject::address;
        address.same_actor = true;
        require((decide(ModerationOperation::ban_address, actor, address).refusal == ModerationRefusal::none) == staff);
        ModerationTarget own;
        own.subject = ModerationSubject::address;
        own.actor_is_creator = true;
        own.role = actor;
        require((decide(ModerationOperation::revoke_ban, actor, own).refusal == ModerationRefusal::none) == staff);
        for (const auto creator : roles) {
            ModerationTarget record;
            record.subject = ModerationSubject::account;
            record.role = creator;
            record.account_bound = true;
            const auto revoked = decide(ModerationOperation::revoke_ban, actor, record);
            const bool allowed = staff && creator != Role::guest && lower(actor, creator);
            require((revoked.refusal == ModerationRefusal::none) == allowed);
        }
    }
    require(decide(ModerationOperation::ban_account, Role::moderator, account(Role::registered)).refusal == ModerationRefusal::missing_permission);
    require(decide(ModerationOperation::ban_address, Role::operator_, session(Role::guest)).refusal == ModerationRefusal::wrong_subject);
    ModerationTarget unbound;
    unbound.subject = ModerationSubject::account;
    require(decide(ModerationOperation::ban_account, Role::owner, unbound).refusal == ModerationRefusal::wrong_subject);
    require(decide(ModerationOperation::revoke_ban, Role::owner, session(Role::registered)).refusal == ModerationRefusal::wrong_subject);
}

void allocation() {
    static_assert(noexcept(security::moderate(ModerationOperation::kick, {}, {}, {})));
    static_assert(noexcept(security::actor_may(ModerationOperation::mute, Role::guest)));
    fail_after = 0;
    const auto decision = decide(ModerationOperation::kick, Role::owner, session(Role::guest));
    fail_after = -1;
    require(decision.refusal == ModerationRefusal::none);
    require(decision.permission == security::Permission::revoke_sessions);
}
}

int main() {
    try {
        catalog();
        sessions();
        bans();
        allocation();
        std::cout << "Moderation permission and target rules passed\n";
    } catch (const std::exception& error) {
        fail_after = -1;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
