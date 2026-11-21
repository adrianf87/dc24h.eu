#pragma once

#include "dc24h/net/server.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>

#include <array>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dc24h::net::test {

namespace asio = boost::asio;
using asio::ip::tcp;
using Error = boost::system::error_code;
using TlsStream = asio::ssl::stream<tcp::socket>;

template <typename Stream> class TestClient {
  public:
    static constexpr bool secure = std::is_same_v<Stream, TlsStream>;

    TestClient()
        requires(!secure)
        : stream_(io_) {}

    explicit TestClient(const std::filesystem::path &trusted_cert, int exact_tls_version = 0)
        requires(secure)
        : stream_(io_, tls_) {
        tls_.load_verify_file(trusted_cert.string());
        stream_.set_verify_mode(asio::ssl::verify_peer);
        stream_.set_verify_callback(asio::ssl::host_name_verification("localhost"));
        if (exact_tls_version != 0) {
            auto *session = stream_.native_handle();
            if (exact_tls_version < TLS1_2_VERSION) {
                SSL_set_security_level(session, 0);
            }
            if (SSL_set_min_proto_version(session, exact_tls_version) != 1 ||
                SSL_set_max_proto_version(session, exact_tls_version) != 1) {
                throw std::runtime_error("Could not select the test client's TLS version");
            }
        }
    }

    Error connect(const Endpoint &endpoint) {
        const tcp::endpoint target(asio::ip::make_address(endpoint.address), endpoint.port);
        return run([&](auto done) { socket().async_connect(target, done); });
    }

    Error handshake()
        requires(secure)
    {
        return run(
            [&](auto done) { stream_.async_handshake(asio::ssl::stream_base::client, done); });
    }

    Error shutdown_tls()
        requires(secure)
    {
        return run([&](auto done) { stream_.async_shutdown(done); });
    }

    void send(std::string_view data) {
        require_success(
            run([&](auto done) { asio::async_write(stream_, asio::buffer(data), done); }));
    }

    std::string line() {
        require_success(run([&](auto done) {
            asio::async_read_until(stream_, asio::dynamic_buffer(input_, 1024 * 1024), '\n', done);
        }));
        const auto size = input_.find('\n') + 1;
        auto text = input_.substr(0, size);
        input_.erase(0, size);
        return text;
    }

    std::string read_prefix(std::size_t size) {
        if (input_.size() < size) {
            require_success(run([&](auto done) {
                asio::async_read(stream_, asio::dynamic_buffer(input_, size),
                                 asio::transfer_exactly(size - input_.size()), done);
            }));
        }
        auto text = input_.substr(0, size);
        input_.erase(0, size);
        return text;
    }

    Error read_to_close(std::string &data) {
        data += std::exchange(input_, {});
        std::array<char, 256> buffer{};
        while (data.size() < 65536) {
            std::size_t count = 0;
            const auto error = run([&](auto done) {
                stream_.async_read_some(asio::buffer(buffer),
                                        [&, done](const Error &result, std::size_t size) {
                                            count = size;
                                            done(result);
                                        });
            });
            data.append(buffer.data(), count);
            if (error) {
                return error;
            }
        }
        throw std::runtime_error("Client reached the test read limit before the peer closed");
    }

    tcp::socket &socket() {
        if constexpr (secure) {
            return stream_.next_layer();
        } else {
            return stream_;
        }
    }

    void close() {
        Error ignored;
        socket().close(ignored);
    }

    void reset() {
        socket().set_option(asio::socket_base::linger(true, 0));
        close();
    }

  private:
    template <typename Start> Error run(Start start) {
        io_.restart();
        asio::steady_timer timer(io_, std::chrono::seconds(4));
        bool expired = false;
        Error result;
        timer.async_wait([&](const Error &error) {
            if (!error) {
                expired = true;
                close();
            }
        });
        start([&](const Error &error, auto...) {
            result = error;
            timer.cancel();
        });
        io_.run();
        if (expired) {
            throw std::runtime_error("Client reached the test I/O deadline");
        }
        return result;
    }

    static void require_success(const Error &error) {
        if (error) {
            throw boost::system::system_error(error);
        }
    }

    asio::io_context io_;
    asio::ssl::context tls_{asio::ssl::context::tls_client};
    Stream stream_;
    std::string input_;
};

using RawClient = TestClient<tcp::socket>;
using TlsClient = TestClient<TlsStream>;

} // namespace dc24h::net::test
