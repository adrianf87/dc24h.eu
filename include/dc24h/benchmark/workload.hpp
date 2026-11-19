#pragma once

#include "dc24h/accounts/account.hpp"
#include "dc24h/security/moderation.hpp"

#include <cstddef>
#include <cstdint>

namespace dc24h::benchmark {

// Fixed in-process work from the alpha-0.8 permission and moderation decisions.
// The counts are the results of those functions. They do not add a permission,
// a route, or a capacity claim. configuration.manage and hub.manage stay unimplemented.
inline constexpr std::uint64_t permission_operations = 1024;
inline constexpr std::uint64_t moderation_operations = 1024;
inline constexpr std::uint64_t benchmark_ceiling_ms = 5000;

struct Sample {
    std::uint64_t operations{};
    std::uint64_t accepted{};
    std::uint64_t elapsed_ms{};
};

struct Report {
    Sample permissions{};
    Sample moderation{};
};

[[nodiscard]] inline accounts::Role benchmark_role(std::uint64_t index) noexcept {
    const auto& roles = accounts::role_catalog;
    return roles[static_cast<std::size_t>(index % roles.size())];
}

[[nodiscard]] inline security::Permission benchmark_permission(std::uint64_t index) noexcept {
    const auto& permissions = security::permission_catalog;
    return permissions[static_cast<std::size_t>(index % permissions.size())];
}

[[nodiscard]] inline security::ModerationOperation benchmark_operation(std::uint64_t index) noexcept {
    const auto& operations = security::moderation_catalog;
    return operations[static_cast<std::size_t>(index % operations.size())];
}

struct BenchmarkModeration {
    security::ModerationOperation operation{};
    security::ModerationActor actor{};
    security::ModerationTarget target{};
};

// Actor is five catalog steps ahead of the target. The subject is the one the
// operation already requires. Account and CID subjects are marked account-bound.
[[nodiscard]] inline BenchmarkModeration benchmark_moderation(std::uint64_t index) noexcept {
    BenchmarkModeration value;
    value.operation = benchmark_operation(index);
    const auto rule = security::moderation_rule(value.operation);
    value.actor.role = benchmark_role(index + 5);
    value.target.subject = rule.subject;
    value.target.role = benchmark_role(index);
    value.target.account_bound = rule.subject == security::ModerationSubject::account ||
                                 rule.subject == security::ModerationSubject::cid;
    return value;
}

// Times the fixed sequences. A zero elapsed_ms means the work finished inside
// one millisecond. The function does not log, allocate a report file, or open a socket.
[[nodiscard]] Report measure() noexcept;

} // namespace dc24h::benchmark
