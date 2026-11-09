#include "dc24h/logging/logger.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utf8proc.h>

#include <algorithm>
#include <limits>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace dc24h::logging {
namespace {
constexpr std::size_t max_message_bytes = 1024;

std::string sanitize(std::string_view text, bool clipped) {
    std::string result;
    result.reserve(max_message_bytes);
    std::size_t offset = 0;
    while (offset < text.size() && result.size() < max_message_bytes) {
        utf8proc_int32_t codepoint{};
        const auto consumed = utf8proc_iterate(
            reinterpret_cast<const utf8proc_uint8_t*>(text.data() + offset),
            static_cast<utf8proc_ssize_t>(std::min<std::size_t>(4, text.size() - offset)), &codepoint);
        if (consumed <= 0) { result += '?'; ++offset; continue; }
        const auto bytes = static_cast<std::size_t>(consumed);
        const auto category = utf8proc_category(codepoint);
        if (category == UTF8PROC_CATEGORY_CC || category == UTF8PROC_CATEGORY_CF ||
            category == UTF8PROC_CATEGORY_ZL || category == UTF8PROC_CATEGORY_ZP) {
            result += '?';
        } else {
            if (result.size() + bytes > max_message_bytes) { break; }
            result.append(text.data() + offset, bytes);
        }
        offset += bytes;
    }
    if (clipped || offset < text.size()) {
        constexpr std::string_view marker = "...[truncated]";
        while (result.size() + marker.size() > max_message_bytes) {
            auto cut = result.size() - 1;
            while (cut > 0 && (static_cast<unsigned char>(result[cut]) & 0xc0) == 0x80) { --cut; }
            result.resize(cut);
        }
        result += marker;
    }
    return result;
}

std::string_view event_name(ConnectionEvent event) noexcept {
    switch (event) {
    case ConnectionEvent::accepted: return "accepted";
    case ConnectionEvent::peer_closed: return "peer_closed";
    case ConnectionEvent::tls_ready: return "tls_ready";
    case ConnectionEvent::tls_handshake_failed: return "tls_handshake_failed";
    case ConnectionEvent::tls_closed: return "tls_closed";
    case ConnectionEvent::tls_read_error: return "tls_read_error";
    case ConnectionEvent::tcp_read_error: return "tcp_read_error";
    case ConnectionEvent::socket_error: return "socket_error";
    case ConnectionEvent::idle_timeout: return "idle_timeout";
    case ConnectionEvent::handshake_timeout: return "handshake_timeout";
    case ConnectionEvent::shutdown: return "shutdown";
    case ConnectionEvent::security_quota: return "security_quota";
    case ConnectionEvent::partial_frame_timeout: return "partial_frame_timeout";
    }
    return {};
}
} // namespace

Logger::Logger(i18n::Language language) : Logger(Options{Level::info, {}, language}) {}

Logger::Logger(Options options)
    : level_(options.level), language_(options.language), connections_(options.connections) {
    if (level_ != Level::info && level_ != Level::warning && level_ != Level::error && level_ != Level::off) {
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
    try {
        const std::lock_guard lock(mutex_);
        report_suppressed_unlocked();
    } catch (...) { /* Best effort during shutdown too. */ }
    if (owned_) { ::close(fd_); }
}

void Logger::info(std::string_view message) noexcept {
    if (level_ == Level::info) { write("INFO", message); }
}
void Logger::warning(std::string_view message) noexcept {
    if (level_ == Level::info || level_ == Level::warning) { write("WARN", message); }
}
void Logger::error(std::string_view message) noexcept {
    if (level_ != Level::off) { write("ERROR", message); }
}

void Logger::write(const char* level, std::string_view message) noexcept {
    // Errors from a log sink must never stop the network or signal shutdown.
    try {
        const std::lock_guard lock(mutex_);
        write_unlocked(level, message);
    } catch (...) {
        // Best effort only.
    }
}

void Logger::write_unlocked(const char* severity, std::string_view message) {
    const auto now = std::time(nullptr);
    std::tm utc{};
    char timestamp[32] = "unknown-time";
    if (::gmtime_r(&now, &utc) != nullptr) {
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    }
    std::string line(timestamp);
    line += " [";
    line += severity;
    line += "] ";
    // Bound allocation and UTF-8 decoding even for an arbitrarily long view.
    // Four lookahead bytes allow the sanitizer to finish a codepoint at the cap.
    const auto clipped = message.substr(0, std::min(message.size(), max_message_bytes + 4));
    const auto localized = i18n::translate(clipped, language_);
    line += sanitize(localized, clipped.size() < message.size());
    line += '\n';
    std::size_t done = 0;
    while (done < line.size()) {
        const auto count = ::write(fd_, line.data() + done, line.size() - done);
        if (count < 0 && errno == EINTR) { continue; }
        if (count <= 0) { break; }
        done += static_cast<std::size_t>(count);
    }
}

void Logger::report_suppressed_unlocked() {
    if (connection_suppressed_ == 0) { return; }
    const auto suppressed = std::exchange(connection_suppressed_, 0);
    write_unlocked("WARN", "Connection log events suppressed=" + std::to_string(suppressed));
}

void Logger::connection(ConnectionEvent event, std::uint64_t id, bool secure) noexcept {
    if (!connection_enabled()) { return; }
    const auto action = event_name(event);
    if (action.empty()) { return; }
    try {
        const std::lock_guard lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        if (!window_started_ || now - connection_window_ >= std::chrono::seconds(1)) {
            report_suppressed_unlocked();
            connection_window_ = now;
            window_started_ = true;
            connection_emitted_ = 0;
        }
        if (connection_emitted_ == 64) {
            if (connection_suppressed_ != std::numeric_limits<std::uint64_t>::max()) { ++connection_suppressed_; }
            return;
        }
        ++connection_emitted_;
        write_unlocked("INFO", "Connection id=" + std::to_string(id) +
                       " transport=" + (secure ? "ADCS" : "ADC") +
                       " event=" + std::string(action));
    } catch (...) { /* Peer logging must not stop the reactor. */ }
}

} // namespace dc24h::logging
