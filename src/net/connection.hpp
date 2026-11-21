#pragma once

#include "dc24h/net/server.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

namespace dc24h::net {

class Connection {
  public:
    virtual ~Connection() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

[[nodiscard]] std::shared_ptr<Connection> make_connection(boost::asio::ip::tcp::socket socket,
                                                          boost::asio::ssl::context *tls,
                                                          ConnectionLimits limits, std::string sid,
                                                          LogSink log,
                                                          std::function<void()> on_closed);

} // namespace dc24h::net
