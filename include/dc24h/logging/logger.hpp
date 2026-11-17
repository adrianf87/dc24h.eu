#pragma once

#include "dc24h/i18n/messages.hpp"
#include "dc24h/chat/statistics.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace dc24h::logging {

enum class Level { info, warning, error, off };
// Fixed internal lifecycle values only: no payloads, nicknames or peer addresses.
enum class ConnectionEvent {
    accepted, peer_closed, tls_ready, tls_handshake_failed, tls_closed,
    tls_read_error, tcp_read_error, socket_error, idle_timeout, handshake_timeout, shutdown, security_quota, partial_frame_timeout,
    login_ready, login_rejected, login_timeout, output_timeout, output_limit, write_error,
    peer_status, close_timeout, negotiation_limit, identity_limit, authentication_revoked, authentication_limit, kicked, banned
};
struct Options {
    Level level{Level::info};
    // Empty means stderr. Otherwise append to a local regular file.
    std::string file_path;
    i18n::Language language{i18n::Language::us};
    bool connections{false};
    bool chat{false};
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
    void warning(std::string_view message) noexcept;
    void error(std::string_view message) noexcept;
    // At most 64 emitted lifecycle events per one-second steady-clock window.
    void connection(ConnectionEvent event, std::uint64_t id, bool secure) noexcept;
    // Independent budget: at most 32 fixed chat outcome records per second.
    void chat_event(chat::Event event) noexcept;
    [[nodiscard]] bool connection_enabled() const noexcept {
        return connections_ && level_ == Level::info;
    }

private:
    void write(const char* level, std::string_view message) noexcept;
    void write_unlocked(const char* severity, std::string_view message);
    void report_suppressed_unlocked();
    void report_chat_suppressed_unlocked();
    std::mutex mutex_;
    Level level_;
    i18n::Language language_;
    int fd_{2};
    bool owned_{false};
    bool connections_;
    bool chat_;
    std::chrono::steady_clock::time_point chat_window_{};
    bool chat_window_started_{false};
    std::size_t chat_emitted_{};
    std::uint64_t chat_suppressed_{};
    std::chrono::steady_clock::time_point connection_window_{};
    bool window_started_{false};
    std::size_t connection_emitted_{0};
    std::uint64_t connection_suppressed_{0};
};

} // namespace dc24h::logging
