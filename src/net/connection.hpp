#pragma once

#include "dc24h/net/server.hpp"

#include "dc24h/hub/presence.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>

namespace dc24h::net {

enum class Continuation { none, resume, admit, reject };
using InputHandler = std::function<void(bool identifying, adc::Message)>;

class Connection {
  public:
    virtual ~Connection() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    // Called on the hub strand. False means the connection cannot accept the batch.
    [[nodiscard]] virtual bool deliver(std::vector<hub::WireFrame> frames,
                                       Continuation after = Continuation::none) = 0;
};

[[nodiscard]] std::shared_ptr<Connection> make_connection(boost::asio::ip::tcp::socket socket,
                                                          boost::asio::ssl::context *tls,
                                                          ConnectionLimits limits, std::string sid,
                                                          LogSink log, InputHandler on_input,
                                                          std::function<void()> on_closed);

} // namespace dc24h::net
