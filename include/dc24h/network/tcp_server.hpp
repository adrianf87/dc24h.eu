#pragma once
#include "dc24h/bans/bans.hpp"
#include "dc24h/admin/kick.hpp"
#include "dc24h/admin/mute.hpp"

#include "dc24h/security/limits.hpp"
#include "dc24h/security/authentication_limits.hpp"
#include "dc24h/security/roles.hpp"
#include "dc24h/users/limits.hpp"
#include "dc24h/chat/settings.hpp"
#include "dc24h/search/security.hpp"
#include "dc24h/transfers/security.hpp"
#include "dc24h/search/statistics.hpp"
#include "dc24h/chat/statistics.hpp"
#include "dc24h/transfers/statistics.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dc24h::logging { class Logger; }
namespace dc24h::adc { class AuthenticationSource; }

namespace dc24h::network {

struct Endpoint {
    std::string address;
    std::uint16_t port;
};

// Numeric IPv4:port or [IPv6]:port. Port 0 requests an ephemeral port.
[[nodiscard]] Endpoint parse_endpoint(std::string_view text);

// Kernel listen queue cap. The kernel may still use a smaller somaxconn.
inline constexpr int listen_backlog = 128;
// Requested SO_RCVBUF and SO_SNDBUF. Linux reports about twice this value.
inline constexpr int socket_buffer_bytes = 65536;
// One plaintext or decrypted read. The ADC decoder has its own frame cap.
inline constexpr std::size_t read_buffer_bytes = 8192;

// Caps the kernel send and receive buffers and sets TCP_USER_TIMEOUT.
// Returns false for a bad descriptor or timeout, or when a setsockopt fails.
// A false result may have applied an earlier option; the caller closes the socket.
[[nodiscard]] bool protect_tcp_socket(int fd, std::chrono::milliseconds user_timeout) noexcept;

struct Options {
    std::vector<Endpoint> listeners{{"127.0.0.1", 1511}};
    std::vector<Endpoint> tls_listeners{};
    std::string tls_certificate;
    std::string tls_private_key;
    std::size_t max_connections{1024};
    std::chrono::seconds idle_timeout{120};
    security::Limits security{};
    security::AuthenticationLimits authentication_limits{};
    security::RolePolicy roles{};
    std::shared_ptr<const bans::Directory> bans{};
    users::Limits users{};
    chat::Settings chat{};
    search::SecuritySettings search{};
    transfers::SecuritySettings transfers{};
    std::shared_ptr<const adc::AuthenticationSource> authentication{};
};

struct Statistics {
    std::size_t active;
    std::uint64_t accepted;
    std::uint64_t rejected;
    std::uint64_t received_bytes;
    std::uint64_t adc_frames;
    std::uint64_t adc_unknown;
    std::uint64_t adc_invalid;
    std::uint64_t adc_keepalives;
    std::uint64_t adcs_handshakes;
    std::uint64_t adcs_incomplete;
    std::uint64_t security_rejected;
    std::uint64_t security_closed;
    std::uint64_t closed{};
    std::uint64_t sent_bytes{};
    std::size_t peak_connections{};
    std::size_t online_users{};
    std::size_t peak_online_users{};
    std::uint64_t successful_logins{};
    chat::Statistics chat{};
    search::Statistics search{};
    transfers::Statistics transfers{};
    admin::KickStatistics kick{};
    admin::MuteStatistics mute{};
};

class TcpServer final {
public:
    explicit TcpServer(logging::Logger& logger, Options options = {});
    ~TcpServer();
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // Blocking and single-use; all sockets are owned by this calling thread.
    // Join run() before destroying this object or its logger.
    void run();
    // Thread-safe, idempotent, retained before run(). Wakes epoll via eventfd.
    void request_stop() noexcept;
    // Atomic counters; fields are independently sampled, not a transaction.
    [[nodiscard]] Statistics statistics() const noexcept;
    // Plain copy of ready sessions. The mutex guards this copy only.
    // Session models stay on the reactor thread.
    [[nodiscard]] users::PresenceDirectory presence() const;

private:
    logging::Logger& logger_;
    Options options_;
    int wake_fd_{-1};
    std::atomic<bool> stop_{false};
    std::atomic<bool> used_{false};
    std::atomic<std::size_t> active_{0};
    std::atomic<std::uint64_t> accepted_{0};
    std::atomic<std::uint64_t> rejected_{0};
    std::atomic<std::uint64_t> received_bytes_{0};
    std::atomic<std::uint64_t> adc_frames_{0};
    std::atomic<std::uint64_t> adc_unknown_{0};
    std::atomic<std::uint64_t> adc_invalid_{0};
    std::atomic<std::uint64_t> adc_keepalives_{0};
    std::atomic<std::uint64_t> adcs_handshakes_{0};
    std::atomic<std::uint64_t> adcs_incomplete_{0};
    std::atomic<std::uint64_t> security_rejected_{0};
    std::atomic<std::uint64_t> security_closed_{0};
    std::atomic<std::uint64_t> closed_{0}, sent_bytes_{0};
    std::atomic<std::size_t> peak_connections_{0}, online_users_{0}, peak_online_users_{0};
    std::atomic<std::uint64_t> successful_logins_{0};
    std::array<std::atomic<std::uint64_t>, chat::statistic_names.size()> chat_statistics_{};
    std::array<std::atomic<std::uint64_t>, search::statistic_names.size()> search_statistics_{};
    std::atomic<std::uint64_t> tracked_searches_{};
    std::array<std::atomic<std::uint64_t>, transfers::statistic_names.size()> transfer_statistics_{};
    std::array<std::atomic<std::uint64_t>, admin::kick_statistic_names.size()> kick_statistics_{};
    std::array<std::atomic<std::uint64_t>, admin::mute_statistic_names.size()> mute_statistics_{};
    mutable std::mutex presence_mu_;
    users::PresenceDirectory presence_{};
};

} // namespace dc24h::network
