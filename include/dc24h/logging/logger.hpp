#pragma once

#include "dc24h/i18n/messages.hpp"

#include <mutex>
#include <string>
#include <string_view>

namespace dc24h::logging {

enum class Level { info, error, off };
struct Options {
    Level level{Level::info};
    // Empty means stderr. Otherwise append to a local regular file.
    std::string file_path;
    i18n::Language language{i18n::Language::us};
};

// Synchronized best-effort UTC logger. New files are created with mode 0600.
// The caller must keep the log directory trusted and join users before destruction.
class Logger final {
public:
    explicit Logger(Options options = {});
    explicit Logger(i18n::Language language);
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void info(std::string_view message) noexcept;
    void error(std::string_view message) noexcept;

private:
    void write(const char* level, std::string_view message) noexcept;
    std::mutex mutex_;
    Level level_;
    i18n::Language language_;
    int fd_{2};
    bool owned_{false};
};

} // namespace dc24h::logging
