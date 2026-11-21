#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace boost::asio {
class io_context;
}

namespace dc24h::net {

struct Endpoint {
    std::string address;
    std::uint16_t port;
};

struct ConnectionLimits {
    std::size_t input_bytes = 16 * 1024;
    std::size_t pending_bytes = 64 * 1024;
    std::chrono::milliseconds setup_timeout{30000};
    std::chrono::milliseconds tls_timeout{10000};
    std::chrono::milliseconds identify_timeout{15000};
    std::chrono::milliseconds idle_timeout{300000};
    std::chrono::milliseconds shutdown_timeout{2000};
};

struct ServerConfig {
    Endpoint adcs{"127.0.0.1", 1511};
    std::optional<Endpoint> adc;
    std::filesystem::path certificate;
    std::filesystem::path private_key;
    std::size_t max_clients = 128;
    ConnectionLimits limits;
    std::size_t max_nick_codepoints = 32;
    std::size_t max_message_bytes = 1024;
};

// Called on an I/O worker. The sink must be thread-safe and return promptly.
using LogSink = std::function<void(std::string_view)>;

class Server {
  public:
    // Binds the listeners before returning. The I/O context must outlive the server.
    Server(boost::asio::io_context &io, ServerConfig config, LogSink log = {});
    ~Server();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    [[nodiscard]] const Endpoint &adcs_endpoint() const noexcept;
    [[nodiscard]] const std::optional<Endpoint> &adc_endpoint() const noexcept;
    [[nodiscard]] std::size_t active_clients() const noexcept;
    [[nodiscard]] std::size_t online_users() const noexcept;
    void stop();

  private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace dc24h::net
