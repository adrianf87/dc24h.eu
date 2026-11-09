#include "dc24h/logging/logger.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace dc24h::logging {

Logger::Logger(i18n::Language language) : Logger(Options{Level::info, {}, language}) {}

Logger::Logger(Options options) : level_(options.level), language_(options.language) {
    if (level_ != Level::info && level_ != Level::error && level_ != Level::off) {
        throw std::invalid_argument("Invalid log level");
    }
    if (options.file_path.empty()) { return; }
    if (options.file_path.find('\0') != std::string::npos) {
        throw std::invalid_argument("Log file path contains a NUL byte");
    }
    const int opened = ::open(options.file_path.c_str(),
                              O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY, 0600);
    if (opened < 0) {
        throw std::system_error(errno, std::generic_category(), "Cannot open configured log file");
    }
    struct stat state{};
    if (::fstat(opened, &state) != 0 || !S_ISREG(state.st_mode) || state.st_nlink != 1) {
        ::close(opened);
        throw std::runtime_error("Configured log destination must be a regular file with one link");
    }
    fd_ = opened;
    owned_ = true;
}

Logger::~Logger() {
    if (owned_) { ::close(fd_); }
}

void Logger::info(std::string_view message) noexcept {
    if (level_ == Level::info) { write("INFO", message); }
}
void Logger::error(std::string_view message) noexcept {
    if (level_ != Level::off) { write("ERROR", message); }
}

void Logger::write(const char* level, std::string_view message) noexcept {
    // Errors from a log sink must never stop the network or signal shutdown.
    try {
        const std::lock_guard lock(mutex_);
        const auto now = std::time(nullptr);
        std::tm utc{};
        char timestamp[32] = "unknown-time";
        if (::gmtime_r(&now, &utc) != nullptr) {
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
        }
        std::string line(timestamp);
        line += " [";
        line += level;
        line += "] ";
        line += i18n::translate(message, language_);
        line += '\n';
        std::size_t done = 0;
        while (done < line.size()) {
            const auto count = ::write(fd_, line.data() + done, line.size() - done);
            if (count < 0 && errno == EINTR) { continue; }
            if (count <= 0) { break; }
            done += static_cast<std::size_t>(count);
        }
    } catch (...) {
        // Best effort only.
    }
}

} // namespace dc24h::logging
