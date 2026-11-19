#include "dc24h/benchmark/workload.hpp"

#include "dc24h/security/moderation.hpp"
#include "dc24h/security/roles.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}

std::uint64_t permission_accepts() {
    std::uint64_t accepted = 0;
    for (std::uint64_t index = 0; index < dc24h::benchmark::permission_operations; ++index) {
        if (dc24h::security::allows(dc24h::benchmark::benchmark_role(index),
                                    dc24h::benchmark::benchmark_permission(index))) {
            ++accepted;
        }
    }
    return accepted;
}

std::uint64_t moderation_accepts() {
    std::uint64_t accepted = 0;
    for (std::uint64_t index = 0; index < dc24h::benchmark::moderation_operations; ++index) {
        const auto input = dc24h::benchmark::benchmark_moderation(index);
        const auto decision = dc24h::security::moderate(input.operation, input.actor, input.target);
        if (decision.refusal == dc24h::security::ModerationRefusal::none) { ++accepted; }
    }
    return accepted;
}

void check(const dc24h::benchmark::Sample& sample, std::uint64_t operations, std::uint64_t accepted,
           const char* name) {
    require(sample.operations == operations, name);
    require(sample.accepted == accepted && accepted > 0 && accepted <= operations, name);
    require(sample.elapsed_ms <= dc24h::benchmark::benchmark_ceiling_ms, name);
}

} // namespace

int main() {
    try {
        const auto report = dc24h::benchmark::measure();
        check(report.permissions, dc24h::benchmark::permission_operations, permission_accepts(),
              "Permission sample drifted");
        check(report.moderation, dc24h::benchmark::moderation_operations, moderation_accepts(),
              "Moderation sample drifted");
        std::cout << "Alpha 0.8 permission and moderation benchmark OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
