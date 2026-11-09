#pragma once

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
    std::size_t max_connections{1024};
    std::chrono::seconds idle_timeout{120};
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
};

} // namespace dc24h::network
