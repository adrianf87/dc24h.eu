#include "dc24h/monitoring/process.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <limits>
#include <optional>

namespace dc24h::monitoring {
namespace {
[[nodiscard]] std::optional<std::uint64_t> parse_u64(std::string_view token) noexcept {
    if (token.empty()) { return std::nullopt; }
    std::uint64_t value = 0;
    for (const char character : token) {
        if (character < '0' || character > '9') { return std::nullopt; }
        const auto digit = static_cast<std::uint64_t>(character - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) { return std::nullopt; }
        value = value * 10 + digit;
    }
    return value;
}

[[nodiscard]] std::uint64_t ticks_to_ms(std::uint64_t ticks, long ticks_per_second) noexcept {
    const auto scale = static_cast<std::uint64_t>(ticks_per_second);
    if (ticks > std::numeric_limits<std::uint64_t>::max() / 1000) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return ticks * 1000 / scale;
}
} // namespace

ProcessSample parse_stat(std::string_view text, long ticks_per_second, long page_size) noexcept {
    if (ticks_per_second <= 0 || page_size <= 0) { return {}; }
    const auto newline = text.find('\n');
    if (newline != std::string_view::npos) { text = text.substr(0, newline); }
    const auto mark = text.rfind(')');
    if (mark == std::string_view::npos || mark + 1 >= text.size()) { return {}; }
    text.remove_prefix(mark + 1);
    // Field 3 is the first token after comm. utime, stime and rss are fields 14, 15 and 24.
    std::array<std::string_view, 22> tokens{};
    std::size_t count = 0;
    std::size_t cursor = 0;
    while (cursor < text.size() && count < tokens.size()) {
        while (cursor < text.size() && text[cursor] == ' ') { ++cursor; }
        if (cursor >= text.size()) { break; }
        const auto start = cursor;
        while (cursor < text.size() && text[cursor] != ' ') { ++cursor; }
        tokens[count++] = text.substr(start, cursor - start);
    }
    if (count < tokens.size()) { return {}; }
    const auto user = parse_u64(tokens[11]);
    const auto system = parse_u64(tokens[12]);
    const auto rss = parse_u64(tokens[21]);
    if (!user || !system || !rss) { return {}; }
    const auto page = static_cast<std::uint64_t>(page_size);
    if (*rss > std::numeric_limits<std::uint64_t>::max() / page) { return {}; }
    ProcessSample sample;
    sample.available = true;
    sample.cpu_user_ms = ticks_to_ms(*user, ticks_per_second);
    sample.cpu_system_ms = ticks_to_ms(*system, ticks_per_second);
    sample.rss_bytes = *rss * page;
    return sample;
}

ProcessSample read_process() noexcept {
    const int fd = ::open("/proc/self/stat", O_RDONLY | O_CLOEXEC);
    if (fd < 0) { return {}; }
    struct Close {
        int fd;
        ~Close() { ::close(fd); }
    } guard{fd};
    std::array<char, 4096> buffer{};
    std::size_t used = 0;
    while (used < buffer.size()) {
        const auto count = ::read(fd, buffer.data() + used, buffer.size() - used);
        if (count < 0) {
            if (errno == EINTR) { continue; }
            return {};
        }
        if (count == 0) { break; }
        used += static_cast<std::size_t>(count);
    }
    if (used == 0 || used == buffer.size()) { return {}; }
    return parse_stat(std::string_view(buffer.data(), used), ::sysconf(_SC_CLK_TCK), ::sysconf(_SC_PAGESIZE));
}
} // namespace dc24h::monitoring
