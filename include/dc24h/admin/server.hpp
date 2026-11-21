#pragma once

#include "dc24h/db/config.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace dc24h::net {
class Server;
}

namespace dc24h::admin {

struct Config {
    std::uint16_t port = 8081;
    std::string origin;
};

// Serves HTTP only on IPv4 loopback. The hub must outlive this listener.
class Server {
  public:
    Server(net::Server &hub, db::Config database, Config config);
    ~Server();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    [[nodiscard]] std::uint16_t port() const noexcept;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dc24h::admin
