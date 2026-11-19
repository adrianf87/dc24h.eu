#include "dc24h/benchmark/workload.hpp"

#include "dc24h/security/roles.hpp"

#include <chrono>
#include <cstdint>

namespace dc24h::benchmark {
namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t milliseconds_since(Clock::time_point started) noexcept {
    const auto elapsed = Clock::now() - started;
    if (elapsed < Clock::duration::zero()) { return 0; }
    const auto count = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    if (count <= 0) { return 0; }
    return static_cast<std::uint64_t>(count);
}

} // namespace

Report measure() noexcept {
    Report report;
    report.permissions.operations = permission_operations;
    report.moderation.operations = moderation_operations;

    volatile std::uint64_t permission_sink = 0;
    const auto permission_started = Clock::now();
    for (std::uint64_t index = 0; index < permission_operations; ++index) {
        const auto allowed = security::allows(benchmark_role(index), benchmark_permission(index));
        permission_sink = permission_sink + (allowed ? std::uint64_t{1} : std::uint64_t{0});
    }
    report.permissions.elapsed_ms = milliseconds_since(permission_started);
    report.permissions.accepted = permission_sink;

    volatile std::uint64_t moderation_sink = 0;
    const auto moderation_started = Clock::now();
    for (std::uint64_t index = 0; index < moderation_operations; ++index) {
        const auto input = benchmark_moderation(index);
        const auto decision = security::moderate(input.operation, input.actor, input.target);
        const auto accepted = decision.refusal == security::ModerationRefusal::none;
        moderation_sink = moderation_sink + (accepted ? std::uint64_t{1} : std::uint64_t{0});
    }
    report.moderation.elapsed_ms = milliseconds_since(moderation_started);
    report.moderation.accepted = moderation_sink;
    return report;
}

} // namespace dc24h::benchmark
