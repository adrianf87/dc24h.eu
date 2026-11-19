#pragma once

#include "dc24h/accounts/account.hpp"
#include "dc24h/security/moderation.hpp"
#include "dc24h/security/roles.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace dc24h::profile {

// Fixed in-process stages from the alpha-0.8 permission, moderation and role
// decisions. The ranking names the slowest stage in this sample. It does not
// add a permission, a route, a sampling profiler or a capacity claim.
// configuration.manage and hub.manage stay unimplemented.
inline constexpr std::size_t stage_count = 3;
inline constexpr std::uint64_t permission_operations = 256;
inline constexpr std::uint64_t moderation_operations = 512;
inline constexpr std::uint64_t decision_operations = 1024;
inline constexpr std::uint64_t profile_ceiling_ms = 5000;

enum class Stage : std::uint8_t { permissions, moderation, decisions };

inline constexpr std::array<Stage, stage_count> stage_catalog{
    Stage::permissions, Stage::moderation, Stage::decisions};

enum class DecisionKind : std::uint8_t { mute, revoke, manage, assign };

struct Span {
    Stage stage{};
    std::uint64_t operations{};
    std::uint64_t accepted{};
    std::uint64_t elapsed_ms{};
};

struct Report {
    std::array<Span, stage_count> spans{};
    std::optional<std::size_t> bottleneck{};
};

struct ProfileModeration {
    security::ModerationOperation operation{};
    security::ModerationActor actor{};
    security::ModerationTarget target{};
};

struct ProfileDecision {
    DecisionKind kind{DecisionKind::mute};
    accounts::Role actor{accounts::Role::guest};
    accounts::Role target{accounts::Role::guest};
    accounts::Role proposed{accounts::Role::guest};
};

[[nodiscard]] inline accounts::Role profile_role(std::uint64_t index) noexcept {
    const auto& roles = accounts::role_catalog;
    return roles[static_cast<std::size_t>(index % roles.size())];
}

[[nodiscard]] inline security::Permission profile_permission(std::uint64_t index) noexcept {
    const auto& permissions = security::permission_catalog;
    return permissions[static_cast<std::size_t>(index % permissions.size())];
}

// Actor is five catalog steps ahead of the target. The subject is the one the
// operation already requires. Account and CID subjects are marked account-bound.
[[nodiscard]] inline ProfileModeration profile_moderation(std::uint64_t index) noexcept {
    ProfileModeration value;
    const auto& operations = security::moderation_catalog;
    value.operation = operations[static_cast<std::size_t>(index % operations.size())];
    const auto rule = security::moderation_rule(value.operation);
    value.actor.role = profile_role(index + 5);
    value.target.subject = rule.subject;
    value.target.role = profile_role(index);
    value.target.account_bound = rule.subject == security::ModerationSubject::account ||
                                 rule.subject == security::ModerationSubject::cid;
    return value;
}

// Cycles mute, kick-rank, account management and role assignment. These calls
// do not dispatch a kick, a mute or a ban.
[[nodiscard]] inline ProfileDecision profile_decision(std::uint64_t index) noexcept {
    constexpr std::array<DecisionKind, 4> kinds{
        DecisionKind::mute, DecisionKind::revoke, DecisionKind::manage, DecisionKind::assign};
    ProfileDecision value;
    value.kind = kinds[static_cast<std::size_t>(index % kinds.size())];
    value.actor = profile_role(index + 5);
    value.target = profile_role(index);
    value.proposed = profile_role(index + 1);
    return value;
}

[[nodiscard]] inline bool decision_accepted(const ProfileDecision& input) noexcept {
    switch (input.kind) {
    case DecisionKind::mute: return security::can_mute(input.actor, input.target);
    case DecisionKind::revoke: return security::can_revoke(input.actor, input.target);
    case DecisionKind::manage: return security::can_manage(input.actor, input.target);
    case DecisionKind::assign:
        return security::can_assign(input.actor, input.target, input.proposed);
    }
    return false;
}

// Greatest elapsed_ms wins. Equal elapsed time uses the greater operation
// count. A remaining tie keeps the earlier span. Accepted counts do not rank.
// An empty range has no bottleneck. Zero milliseconds is a tie, not a gap.
[[nodiscard]] inline std::optional<std::size_t> select_bottleneck(const Span* spans,
                                                                  std::size_t count) noexcept {
    if (spans == nullptr || count == 0) { return std::nullopt; }
    std::size_t best = 0;
    for (std::size_t index = 1; index < count; ++index) {
        const Span& candidate = spans[index];
        const Span& current = spans[best];
        if (candidate.elapsed_ms > current.elapsed_ms ||
            (candidate.elapsed_ms == current.elapsed_ms &&
             candidate.operations > current.operations)) {
            best = index;
        }
    }
    return best;
}

// Times the fixed stages and names the bottleneck. A zero elapsed_ms means the
// stage finished inside one millisecond. The function does not log, allocate a
// report file, or open a socket.
[[nodiscard]] Report profile() noexcept;

} // namespace dc24h::profile
