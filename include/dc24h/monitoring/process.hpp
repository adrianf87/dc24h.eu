#pragma once

#include <cstdint>
#include <string_view>

namespace dc24h::monitoring {
// One reading of this process. available is false when the record cannot be used.
// cpu times are cumulative user and system milliseconds. rss_bytes is resident
// pages times the page size. This is not host CPU, a cgroup limit or a history.
struct ProcessSample {
    bool available{false};
    std::uint64_t cpu_user_ms{0};
    std::uint64_t cpu_system_ms{0};
    std::uint64_t rss_bytes{0};
};

// text is one /proc stat record. ticks_per_second and page_size come from sysconf.
// A short record, a non-numeric CPU or RSS field, or a non-positive scale yields
// available=false and zeros.
[[nodiscard]] ProcessSample parse_stat(std::string_view text, long ticks_per_second, long page_size) noexcept;

// Reads only /proc/self/stat. Never throws and never logs the record.
[[nodiscard]] ProcessSample read_process() noexcept;
} // namespace dc24h::monitoring
