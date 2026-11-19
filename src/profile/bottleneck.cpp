#include "dc24h/profile/bottleneck.hpp"

#include <chrono>
#include <cstdint>

namespace dc24h::profile {
namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t milliseconds_since(Clock::time_point started) noexcept {
    const auto elapsed = Clock::now() - started;
    if (elapsed < Clock::duration::zero()) { return 0; }
    const auto count = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    if (count <= 0) { return 0; }
    return static_cast<std::uint64_t>(count);
}

template <typename Body>
Span time_stage(Stage stage, std::uint64_t operations, Body&& body) noexcept {
    Span span;
    span.stage = stage;
    span.operations = operations;
    volatile std::uint64_t sink = 0;
    const auto started = Clock::now();
    for (std::uint64_t index = 0; index < operations; ++index) {
        const auto accepted = body(index);
        sink = sink + (accepted ? std::uint64_t{1} : std::uint64_t{0});
    }
    span.elapsed_ms = milliseconds_since(started);
    span.accepted = sink;
    return span;
}

} // namespace

Report profile() noexcept {
    Report report;
    report.spans[0] = time_stage(Stage::permissions, permission_operations, [](std::uint64_t index) noexcept {
        return security::allows(profile_role(index), profile_permission(index));
    });
    report.spans[1] = time_stage(Stage::moderation, moderation_operations, [](std::uint64_t index) noexcept {
        const auto input = profile_moderation(index);
        const auto decision = security::moderate(input.operation, input.actor, input.target);
        return decision.refusal == security::ModerationRefusal::none;
    });
    report.spans[2] = time_stage(Stage::decisions, decision_operations, [](std::uint64_t index) noexcept {
        return decision_accepted(profile_decision(index));
    });
    report.bottleneck = select_bottleneck(report.spans.data(), report.spans.size());
    return report;
}

} // namespace dc24h::profile
