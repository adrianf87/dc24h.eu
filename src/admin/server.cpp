#include "dc24h/admin/server.hpp"

#include "service.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>

#include <thread>
#include <utility>

namespace dc24h::admin {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
using asio::ip::tcp;
using Error = boost::system::error_code;
using namespace std::chrono_literals;

constexpr std::size_t max_connections = 32;
constexpr unsigned max_header_bytes = 8192;
constexpr unsigned max_body_bytes = 8192;

} // namespace

class Server::Impl {
  public:
    Impl(net::Server &hub, db::Config database, Config config)
        : service_(hub, std::move(database), std::move(config.origin)),
          acceptor_(io_, {asio::ip::address_v4::loopback(), config.port}),
          port_(acceptor_.local_endpoint().port()) {
        accept();
        thread_ = std::jthread([this] {
            try {
                io_.run();
            } catch (const std::exception &) {
                io_.stop();
            }
        });
    }

    ~Impl() {
        io_.stop();
        thread_.join();
        workers_.stop();
        workers_.join();
    }

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

  private:
    class Session : public std::enable_shared_from_this<Session> {
      public:
        Session(Impl &owner, tcp::socket socket) : owner_(owner), stream_(std::move(socket)) {
            request_.header_limit(max_header_bytes);
            request_.body_limit(max_body_bytes);
        }

        void start() {
            stream_.expires_after(5s);
            http::async_read(
                stream_, buffer_, request_,
                [self = shared_from_this()](Error error, std::size_t) { self->read(error); });
        }

      private:
        void read(Error error) {
            if (error) {
                using Status = http::status;
                if (error == http::error::body_limit) {
                    write(error_response(Status::payload_too_large, "payload_too_large",
                                         "Request body exceeds the size limit"));
                } else if (error == http::error::header_limit ||
                           error == http::error::buffer_overflow) {
                    write(error_response(Status::request_header_fields_too_large,
                                         "headers_too_large",
                                         "Request headers exceed the size limit"));
                } else if (error == http::error::end_of_stream || error == beast::error::timeout ||
                           error == asio::error::operation_aborted ||
                           error == asio::error::connection_reset) {
                    close();
                } else {
                    write(error_response(Status::bad_request, "invalid_http",
                                         "Invalid HTTP request"));
                }
                return;
            }
            // Hold the connection slot across the job and its response.
            asio::post(owner_.workers_, [self = shared_from_this(), request = request_.release()] {
                auto response = self->owner_.service_.respond(request);
                asio::post(self->owner_.io_, [self, response = std::move(response)]() mutable {
                    self->write(std::move(response));
                });
            });
        }

        void write(Response response) {
            response_ = std::move(response);
            stream_.expires_after(5s);
            http::async_write(stream_, response_,
                              [self = shared_from_this()](Error, std::size_t) { self->close(); });
        }

        void close() {
            Error ignored;
            stream_.socket().shutdown(tcp::socket::shutdown_both, ignored);
            stream_.socket().close(ignored);
            --owner_.active_;
        }

        Impl &owner_;
        beast::tcp_stream stream_;
        beast::flat_buffer buffer_{max_header_bytes};
        http::request_parser<http::string_body> request_;
        Response response_;
    };

    void accept() {
        acceptor_.async_accept([this](Error error, tcp::socket socket) {
            if (error) {
                retry_.expires_after(100ms);
                retry_.async_wait([this](Error result) {
                    if (!result) {
                        accept();
                    }
                });
                return;
            }
            if (active_ < max_connections) {
                ++active_;
                std::make_shared<Session>(*this, std::move(socket))->start();
            }
            accept();
        });
    }

    Service service_;
    asio::io_context io_;
    tcp::acceptor acceptor_;
    asio::steady_timer retry_{io_};
    asio::thread_pool workers_{2};
    std::uint16_t port_;
    std::size_t active_ = 0;
    std::jthread thread_;
};

Server::Server(net::Server &hub, db::Config database, Config config)
    : impl_(std::make_unique<Impl>(hub, std::move(database), std::move(config))) {}

Server::~Server() = default;

std::uint16_t Server::port() const noexcept { return impl_->port(); }

} // namespace dc24h::admin
