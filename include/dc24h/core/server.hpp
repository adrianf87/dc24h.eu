#pragma once

#include <condition_variable>
#include <mutex>

namespace dc24h::logging { class Logger; }

namespace dc24h::network { class TcpServer; }

namespace dc24h::core {

enum class State { created, running, stopping, stopped };

class Server final {
public:
    // Optional transport and logger must outlive this Core.
    explicit Server(logging::Logger& logger, network::TcpServer* network = nullptr) noexcept;
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Blocking, single-use lifecycle. The caller must join run() before destruction.
    void run();
    // Thread-safe and idempotent; valid even before run().
    void request_stop();
    [[nodiscard]] State state() const;

private:
    logging::Logger& logger_;
    network::TcpServer* network_;
    mutable std::mutex mutex_;
    std::condition_variable wakeup_;
    State state_{State::created};
    bool run_started_{false};
    bool stop_requested_{false};
};

} // namespace dc24h::core
