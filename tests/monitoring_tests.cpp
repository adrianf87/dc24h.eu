#include "dc24h/monitoring/process.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}

std::string record(std::string after_comm) {
    return "42 (dc 24h (hub)) " + after_comm + "\n";
}

std::string tail() {
    // 22 tokens after comm. utime is token 12 (index 11): 100. stime 50. rss 2.
    // Indexes: 0 state, 1..10 integers, 11 utime, 12 stime, 13..20 integers, 21 rss.
    return "R 1 1 1 0 -1 0 0 0 0 0 100 50 0 0 20 0 1 0 0 4096 2 3 4";
}

void parser() {
    const auto sample = dc24h::monitoring::parse_stat(record(tail()), 100, 4096);
    require(sample.available, "well-formed record unavailable");
    require(sample.cpu_user_ms == 1000 && sample.cpu_system_ms == 500 && sample.rss_bytes == 8192,
            "tick or page conversion");
    require(!dc24h::monitoring::parse_stat("", 100, 4096).available, "empty record accepted");
    require(!dc24h::monitoring::parse_stat("42 (only-comm)\n", 100, 4096).available, "short record accepted");
    require(!dc24h::monitoring::parse_stat(record("R 1"), 100, 4096).available, "truncated fields accepted");
    auto bad_cpu = tail();
    bad_cpu.replace(bad_cpu.find("100"), 3, "10x");
    require(!dc24h::monitoring::parse_stat(record(bad_cpu), 100, 4096).available, "non-numeric cpu accepted");
    require(!dc24h::monitoring::parse_stat(record(tail()), 0, 4096).available, "zero tick rate accepted");
    require(!dc24h::monitoring::parse_stat(record(tail()), 100, 0).available, "zero page size accepted");
    auto huge = tail();
    huge.replace(huge.rfind('2'), 1, "18446744073709551615");
    require(!dc24h::monitoring::parse_stat(record(huge), 100, 4096).available, "rss overflow accepted");
    const auto live = dc24h::monitoring::read_process();
    require(live.available && live.rss_bytes > 0, "live process sample");
    const auto again = dc24h::monitoring::read_process();
    require(again.available && again.cpu_user_ms >= live.cpu_user_ms && again.cpu_system_ms >= live.cpu_system_ms,
            "cpu time went backwards");
}
} // namespace

int main() {
    try {
        parser();
        std::cout << "Process sample parsing and live /proc stat OK\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
