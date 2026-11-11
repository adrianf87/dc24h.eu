#pragma once

#include "dc24h/security/limits.hpp"
#include "dc24h/users/limits.hpp"
#include "dc24h/chat/settings.hpp"
#include "dc24h/chat/statistics.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dc24h::logging { class Logger; }

namespace dc24h::network {

struct Endpoint {
    std::string address;
    std::uint16_t port;
};

// Numeric IPv4:port or [IPv6]:port. Port 0 requests an ephemeral port.
[[nodiscard]] Endpoint parse_endpoint(std::string_view text);

struct Options {
    std::vector<Endpoint> listeners{{"127.0.0.1", 1511}};
    std::vector<Endpoint> tls_listeners{};
    std::string tls_certificate;
    std::string tls_private_key;
    std::size_t max_connections{1024};
    std::chrono::seconds idle_timeout{120};
    security::Limits security{};
    users::Limits users{};
    chat::Settings chat{};
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
};

} // namespace dc24h::network
