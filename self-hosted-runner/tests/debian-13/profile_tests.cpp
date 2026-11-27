#include "dc24h/profile/bottleneck.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}

void check_selector() {
    using dc24h::profile::Span;
    using dc24h::profile::Stage;
    using dc24h::profile::select_bottleneck;

    Span storage[1]{};
    require(!select_bottleneck(nullptr, 0).has_value(), "Null empty range");
    require(!select_bottleneck(nullptr, 3).has_value(), "Null range");
    require(!select_bottleneck(storage, 0).has_value(), "Zero count");
    require(select_bottleneck(storage, 1) == std::size_t{0}, "Single span");

    const Span time_wins[3]{
        {Stage::permissions, 1, 0, 8},
        {Stage::moderation, 1000, 1000, 7},
        {Stage::decisions, 1000, 1000, 2},
    };
    require(select_bottleneck(time_wins, 3) == std::size_t{0}, "Elapsed time wins");

    const Span work_wins[3]{
        {Stage::permissions, 10, 10, 0},
        {Stage::moderation, 30, 0, 0},
        {Stage::decisions, 20, 20, 0},
    };
    require(select_bottleneck(work_wins, 3) == std::size_t{1}, "Operation count breaks a time tie");

    const Span earlier_wins[3]{
        {Stage::permissions, 10, 1, 4},
        {Stage::moderation, 10, 100, 4},
        {Stage::decisions, 10, 1000, 4},
    };
    require(select_bottleneck(earlier_wins, 3) == std::size_t{0}, "Accepted count does not rank");
}

std::uint64_t permission_accepts() {
    std::uint64_t accepted = 0;
    for (std::uint64_t index = 0; index < dc24h::profile::permission_operations; ++index) {
        if (dc24h::security::allows(dc24h::profile::profile_role(index),
                                    dc24h::profile::profile_permission(index))) {
            ++accepted;
        }
    }
    return accepted;
}

std::uint64_t moderation_accepts() {
    std::uint64_t accepted = 0;
    for (std::uint64_t index = 0; index < dc24h::profile::moderation_operations; ++index) {
        const auto input = dc24h::profile::profile_moderation(index);
        const auto decision = dc24h::security::moderate(input.operation, input.actor, input.target);
        if (decision.refusal == dc24h::security::ModerationRefusal::none) { ++accepted; }
    }
    return accepted;
}

std::uint64_t decision_accepts() {
    std::uint64_t accepted = 0;
    for (std::uint64_t index = 0; index < dc24h::profile::decision_operations; ++index) {
        if (dc24h::profile::decision_accepted(dc24h::profile::profile_decision(index))) { ++accepted; }
    }
    return accepted;
}

void check_span(const dc24h::profile::Span& span, dc24h::profile::Stage stage, std::uint64_t operations,
                std::uint64_t accepted, const char* name) {
    require(span.stage == stage && span.operations == operations, name);
    require(span.accepted == accepted && accepted > 0 && accepted <= operations, name);
    require(span.elapsed_ms <= dc24h::profile::profile_ceiling_ms, name);
}

} // namespace

int main() {
    try {
        check_selector();
        const auto report = dc24h::profile::profile();
        require(report.spans.size() == dc24h::profile::stage_count, "Stage count drifted");
        check_span(report.spans[0], dc24h::profile::Stage::permissions,
                   dc24h::profile::permission_operations, permission_accepts(), "Permission stage drifted");
        check_span(report.spans[1], dc24h::profile::Stage::moderation,
                   dc24h::profile::moderation_operations, moderation_accepts(), "Moderation stage drifted");
        check_span(report.spans[2], dc24h::profile::Stage::decisions,
                   dc24h::profile::decision_operations, decision_accepts(), "Decision stage drifted");
        const auto selected = dc24h::profile::select_bottleneck(report.spans.data(), report.spans.size());
        require(selected.has_value() && report.bottleneck == selected, "Bottleneck drifted");
        require(*report.bottleneck < dc24h::profile::stage_count, "Bottleneck index drifted");
        const bool same_elapsed = report.spans[0].elapsed_ms == report.spans[1].elapsed_ms &&
                                  report.spans[1].elapsed_ms == report.spans[2].elapsed_ms;
        if (same_elapsed) { require(*report.bottleneck == std::size_t{2}, "Work rank drifted"); }
        std::cout << "Alpha 0.8 bottleneck profile OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
