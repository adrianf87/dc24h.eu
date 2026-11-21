#pragma once

#include "dc24h/db/config.hpp"
#include "dc24h/hub/presence.hpp"
#include "dc24h/net/config.hpp"

#include <functional>
#include <memory>

namespace boost::asio {
class io_context;
}

namespace dc24h::net {

class BanWorker;

// Called on an I/O worker. The sink must be thread-safe and return promptly.
using LogSink = std::function<void(std::string_view)>;

class Server {
  public:
    // Binds the listeners before returning. The I/O context must outlive the server.
    // Destroy on the owner thread so joining the database worker stays off the network loop.
    Server(boost::asio::io_context &io, ServerConfig config, LogSink log = {},
           std::optional<db::Config> database = std::nullopt);
    ~Server();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    [[nodiscard]] const Endpoint &adcs_endpoint() const noexcept;
    [[nodiscard]] const std::optional<Endpoint> &adc_endpoint() const noexcept;
    [[nodiscard]] std::size_t active_clients() const noexcept;
    [[nodiscard]] std::size_t online_users() const noexcept;
    // The callback runs on the control strand when the server accepts the request.
    using UsersHandler = std::function<void(std::vector<hub::OnlineUser>)>;
    [[nodiscard]] bool read_users(std::string after, std::size_t limit, UsersHandler reply) const;
    void stop();

  private:
    class Impl;
    std::shared_ptr<BanWorker> bans_;
    std::shared_ptr<Impl> impl_;
};

} // namespace dc24h::net
