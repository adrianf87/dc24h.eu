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
constexpr std::size_t max_pending_records = 256;

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

std::string log_line(const char* severity, std::string_view message, i18n::Language language) {
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
    const auto clipped = message.substr(0, std::min(message.size(), max_message_bytes + 4));
    line += sanitize(i18n::translate(clipped, language), clipped.size() < message.size());
    line += '\n';
    return line;
}

void output_line(int fd, const std::string& line) noexcept {
    std::size_t done = 0;
    while (done < line.size()) {
        const auto count = ::write(fd, line.data() + done, line.size() - done);
        if (count < 0 && errno == EINTR) { continue; }
        if (count <= 0) { break; }
        done += static_cast<std::size_t>(count);
    }
}

bool security_outcome(ConnectionEvent event) noexcept {
    switch (event) {
    case ConnectionEvent::tls_handshake_failed:
    case ConnectionEvent::handshake_timeout:
    case ConnectionEvent::security_quota:
    case ConnectionEvent::partial_frame_timeout:
    case ConnectionEvent::login_rejected:
    case ConnectionEvent::login_timeout:
    case ConnectionEvent::negotiation_limit:
    case ConnectionEvent::identity_limit:
    case ConnectionEvent::authentication_revoked:
    case ConnectionEvent::authentication_limit:
    case ConnectionEvent::kicked:
    case ConnectionEvent::banned:
        return true;
    default:
        return false;
    }
}

int open_log_file(const std::string& path) {
    if (path.find('\0') != std::string::npos) {
        throw std::invalid_argument("Log file path contains a NUL byte");
    }
    const int created = ::open(path.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY, 0600);
    if (created >= 0) {
        struct stat state{};
        const bool private_file = ::fchmod(created, 0600) == 0 && ::fstat(created, &state) == 0 &&
            S_ISREG(state.st_mode) && state.st_nlink == 1 && state.st_uid == ::geteuid() &&
            (state.st_mode & 0777) == 0600;
        if (!private_file) {
            ::close(created);
            ::unlink(path.c_str());
            throw std::runtime_error("Configured log file must be private");
        }
        return created;
    }
    if (errno != EEXIST) {
        throw std::system_error(errno, std::generic_category(), "Cannot open configured log file");
    }
    const int opened = ::open(path.c_str(),
        O_WRONLY | O_APPEND | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY);
    if (opened < 0) {
        throw std::system_error(errno, std::generic_category(), "Cannot open configured log file");
    }
    struct stat state{};
    if (::fstat(opened, &state) != 0 || !S_ISREG(state.st_mode) || state.st_nlink != 1) {
        ::close(opened);
        throw std::runtime_error("Configured log destination must be a regular file with one link");
    }
    if (state.st_uid != ::geteuid() || (state.st_mode & 0777) != 0600) {
        ::close(opened);
        throw std::runtime_error("Configured log file must be private");
    }
    return opened;
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
    case ConnectionEvent::login_ready: return "login_ready";
    case ConnectionEvent::login_rejected: return "login_rejected";
    case ConnectionEvent::login_timeout: return "login_timeout";
    case ConnectionEvent::output_timeout: return "output_timeout";
    case ConnectionEvent::output_limit: return "output_limit";
    case ConnectionEvent::write_error: return "write_error";
    case ConnectionEvent::peer_status: return "peer_status";
    case ConnectionEvent::close_timeout: return "close_timeout";
    case ConnectionEvent::negotiation_limit: return "negotiation_limit";
    case ConnectionEvent::authentication_limit: return "authentication_limit";
    case ConnectionEvent::banned: return "banned";
    case ConnectionEvent::kicked: return "kicked";
    case ConnectionEvent::authentication_revoked: return "authentication_revoked";
    case ConnectionEvent::identity_limit: return "identity_limit";
    }
    return {};
}
} // namespace

Logger::Logger(i18n::Language language) : Logger(Options{Level::info, {}, language}) {}

Logger::Logger(Options options)
    : level_(options.level), language_(options.language), connections_(options.connections), chat_(options.chat) {
    if (level_ != Level::info && level_ != Level::warning && level_ != Level::error && level_ != Level::off) {
        throw std::invalid_argument("Invalid log level");
    }
    if (!options.file_path.empty()) {
        fd_ = open_log_file(options.file_path);
        owned_ = true;
    }
    try { writer_ = std::jthread([this] { writer_loop(); }); }
    catch (...) {
        if (owned_) { ::close(fd_); owned_ = false; }
        throw;
    }
}

Logger::~Logger() {
    try {
        const std::lock_guard lock(mutex_);
        report_suppressed_unlocked();
        report_chat_suppressed_unlocked();
        report_security_suppressed_unlocked();
        stopping_ = true;
    } catch (...) { /* Best effort during shutdown too. */ }
    { const std::lock_guard lock(mutex_); stopping_ = true; }
    wake_.notify_one();
    if (writer_.joinable()) { writer_.join(); }
    if (owned_) { ::close(fd_); }
}

void Logger::flush() {
    std::unique_lock lock(mutex_);
    drained_.wait(lock, [this] { return pending_.empty() && !writing_ && queue_dropped_ == 0; });
}

void Logger::writer_loop() noexcept {
    for (;;) {
        std::string line;
        try {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !pending_.empty() || queue_dropped_ != 0; });
            if (!pending_.empty()) {
                line = std::move(pending_.front());
                pending_.pop_front();
            } else if (queue_dropped_ != 0) {
                const auto count = std::exchange(queue_dropped_, 0);
                line = log_line("WARN", "Log queue records dropped=" + std::to_string(count), language_);
            } else if (stopping_) {
                drained_.notify_all();
                return;
            }
            writing_ = true;
        } catch (...) {
            // Allocation failure in a best-effort logger must not terminate
            // the process. The next wakeup or shutdown can retry the drain.
            std::lock_guard lock(mutex_);
            queue_dropped_ = 0;
            writing_ = false;
            drained_.notify_all();
            if (stopping_ && pending_.empty()) return;
            continue;
        }
        output_line(fd_, line); // Only the worker performs potentially blocking I/O.
        {
            const std::lock_guard lock(mutex_);
            writing_ = false;
            if (pending_.empty() && queue_dropped_ == 0) { drained_.notify_all(); }
        }
    }
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
    // Caller holds mutex_; no filesystem/pipe writes on the reactor thread.
    if (pending_.size() >= max_pending_records) {
        if (queue_dropped_ != std::numeric_limits<std::uint64_t>::max()) { ++queue_dropped_; }
        wake_.notify_one();
        return;
    }
    pending_.push_back(log_line(severity, message, language_));
    wake_.notify_one();
}

void Logger::report_suppressed_unlocked() {
    if (connection_suppressed_ == 0) { return; }
    const auto suppressed = std::exchange(connection_suppressed_, 0);
    write_unlocked("WARN", "Connection log events suppressed=" + std::to_string(suppressed));
}

void Logger::report_security_suppressed_unlocked() {
    if (security_suppressed_ == 0) { return; }
    const auto suppressed = std::exchange(security_suppressed_, 0);
    write_unlocked("WARN", "Suppressed security records=" + std::to_string(suppressed));
}

void Logger::record_security_unlocked(std::string_view action) {
    if (level_ != Level::info && level_ != Level::warning) { return; }
    const auto now = std::chrono::steady_clock::now();
    if (!security_window_started_ || now - security_window_ >= std::chrono::seconds(1)) {
        report_security_suppressed_unlocked();
        security_window_ = now;
        security_window_started_ = true;
        security_emitted_ = 0;
    }
    if (security_emitted_ == 32) {
        if (security_suppressed_ != std::numeric_limits<std::uint64_t>::max()) { ++security_suppressed_; }
        return;
    }
    ++security_emitted_;
    write_unlocked("WARN", "Security record=" + std::string(action));
}

void Logger::connection(ConnectionEvent event, std::uint64_t id, bool secure) noexcept {
    const auto action = event_name(event);
    if (action.empty()) { return; }
    try {
        const std::lock_guard lock(mutex_);
        if (security_outcome(event)) { record_security_unlocked(action); }
        if (!connection_enabled()) { return; }
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

void Logger::report_chat_suppressed_unlocked() {
    if (chat_suppressed_ == 0) { return; }
    const auto count = std::exchange(chat_suppressed_, 0);
    write_unlocked("WARN", "Chat log events suppressed=" + std::to_string(count));
}
void Logger::chat_event(chat::Event event) noexcept {
    const auto index = static_cast<std::size_t>(event);
    if (!chat_ || level_ != Level::info || index >= chat::event_names.size()) { return; }
    try {
        const std::lock_guard lock(mutex_);
        const auto now = std::chrono::steady_clock::now();
        if (!chat_window_started_ || now - chat_window_ >= std::chrono::seconds(1)) {
            report_chat_suppressed_unlocked();
            chat_window_ = now;
            chat_window_started_ = true;
            chat_emitted_ = 0;
        }
        if (chat_emitted_ == 32) { chat::add_saturating(chat_suppressed_); return; }
        ++chat_emitted_;
        write_unlocked("INFO", "Chat event=" + std::string(chat::event_names[index]));
    } catch (...) { /* Best effort; accounting and dispatch never depend on I/O. */ }
}
} // namespace dc24h::logging
