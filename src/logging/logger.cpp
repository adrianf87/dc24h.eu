#include "dc24h/logging/logger.hpp"

#include <cstdio>
#include <ctime>

namespace dc24h::logging {

void Logger::info(std::string_view message) noexcept { write("INFO", message); }
void Logger::error(std::string_view message) noexcept { write("ERROR", message); }

void Logger::write(const char* level, std::string_view message) noexcept {
    // Logging must not prevent shutdown, even when the output sink fails.
    try {
        const std::lock_guard lock(mutex_);
        const auto now = std::time(nullptr);
        std::tm utc{};
        char timestamp[32] = "unknown-time";
        if (::gmtime_r(&now, &utc) != nullptr) {
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
        }
        const auto localized = i18n::translate(message, language_);
        std::fprintf(stderr, "%s [%s] ", timestamp, level);
        std::fwrite(localized.data(), 1, localized.size(), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    } catch (...) {
        // Best effort only; never throw from an error reporting path.
    }
}

} // namespace dc24h::logging
