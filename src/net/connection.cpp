#include "connection.hpp"

#include "dc24h/adc/parser.hpp"
#include "dc24h/adc/session.hpp"
#include "dc24h/version.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>

#include <algorithm>
#include <array>
#include <deque>
#include <type_traits>
#include <utility>
#include <vector>

namespace dc24h::net {
namespace {

namespace asio = boost::asio;
using asio::ip::tcp;
using TlsStream = asio::ssl::stream<tcp::socket>;
using Error = boost::system::error_code;
using Clock = std::chrono::steady_clock;

constexpr std::size_t read_chunk_bytes = 4096;

enum class Phase { establishing, open, closing, closed };

std::string hub_information() {
    std::string line = "IINF CT32 NI";
    line += project_name;
    line += " VE";
    line += version;
    line += '\n';
    return line;
}

std::string client_information(const adc::Message &message, const std::string &sid,
                               const std::optional<asio::ip::address> &peer) {
    std::string line = "BINF " + sid;
    for (const auto &field : message.parameters) {
        const auto name = std::string_view(field).substr(0, 2);
        if (name == "PD") {
            continue;
        }
        if (name == "I4" || name == "I6") {
            if (peer && ((name == "I4" && peer->is_v4()) || (name == "I6" && peer->is_v6()))) {
                line += ' ';
                line += name;
                line += peer->to_string();
            }
            continue;
        }
        line += ' ';
        line += adc::escape_parameter(field);
    }
    line += '\n';
    return line;
}

template <typename Stream>
class StreamConnection final : public Connection,
                               public std::enable_shared_from_this<StreamConnection<Stream>> {
  public:
    StreamConnection(Stream stream, ConnectionLimits limits, std::string sid, LogSink log,
                     std::function<void()> on_closed)
        : stream_(std::move(stream)), executor_(stream_.get_executor()), timer_(executor_),
          limits_(limits), setup_deadline_(Clock::now() + limits.setup_timeout),
          sid_(std::move(sid)), protocol_(sid_), decoder_({.frame_bytes = limits.input_bytes}),
          log_(std::move(log)), on_closed_(std::move(on_closed)) {}

    void start() override {
        asio::post(executor_, [self = this->shared_from_this()] { self->establish(); });
    }

    void stop() override {
        asio::post(executor_, [self = this->shared_from_this()] {
            self->begin_close("Server shutdown", true);
        });
    }

  private:
    static constexpr bool secure = std::is_same_v<Stream, TlsStream>;

    tcp::socket &socket() {
        if constexpr (secure) {
            return stream_.next_layer();
        } else {
            return stream_;
        }
    }

    void report(std::string_view event, const Error &error = {}) const {
        if (log_) {
            std::string line = secure ? "ADCS [" : "ADC [";
            line += sid_;
            line += "] ";
            line += event;
            if (error) {
                line += ": ";
                line += error.message();
            }
            log_(line);
        }
    }

    void establish() {
        if (phase_ != Phase::establishing) {
            return;
        }
        Error error;
        auto endpoint = socket().remote_endpoint(error);
        if (!error) {
            peer_address_ = endpoint.address();
            if (peer_address_->is_v6()) {
                auto address = peer_address_->to_v6();
                address.scope_id(0);
                peer_address_ = address;
            }
        }
        socket().set_option(tcp::no_delay(true), error);
        report("Connection accepted");
        if constexpr (secure) {
            setup_timer(limits_.tls_timeout, "TLS handshake timeout");
            stream_.async_handshake(asio::ssl::stream_base::server,
                                    [self = this->shared_from_this()](const Error &result) {
                                        if (self->phase_ != Phase::establishing) {
                                            return;
                                        }
                                        if (result) {
                                            self->close_now("TLS handshake failed", result);
                                            return;
                                        }
                                        self->report("TLS handshake completed");
                                        self->open_protocol();
                                    });
        } else {
            open_protocol();
        }
    }

    void open_protocol() {
        phase_ = Phase::open;
        setup_timer(limits_.identify_timeout, "ADC identification timeout");
        read_next();
    }

    void setup_timer(std::chrono::milliseconds limit, std::string_view reason) {
        const auto phase_deadline = Clock::now() + limit;
        if (setup_deadline_ < phase_deadline) {
            arm_timer(setup_deadline_, "Connection setup timeout");
        } else {
            arm_timer(phase_deadline, reason);
        }
    }

    void arm_timer(Clock::time_point deadline, std::string_view reason, bool graceful = false) {
        const auto generation = ++timer_generation_;
        timer_.expires_at(deadline);
        timer_.async_wait([self = this->shared_from_this(), generation, reason,
                           graceful](const Error &error) {
            if (error || self->phase_ == Phase::closed || generation != self->timer_generation_) {
                return;
            }
            if (graceful) {
                self->begin_close(reason, true);
            } else {
                self->close_now(reason);
            }
        });
    }

    void read_next() {
        read_pending_ = true;
        const auto size = std::min(limits_.input_bytes, read_buffer_.size());
        stream_.async_read_some(
            asio::buffer(read_buffer_.data(), size),
            [self = this->shared_from_this()](const Error &error, std::size_t count) {
                self->read_complete(error, count);
            });
    }

    void read_complete(const Error &error, std::size_t count) {
        read_pending_ = false;
        if (phase_ == Phase::closed) {
            return;
        }
        if (phase_ == Phase::closing) {
            if (error && error != asio::error::operation_aborted && error != asio::error::eof) {
                close_now("Read failed during shutdown", error);
            } else {
                finish_close();
            }
            return;
        }
        if (error) {
            if (error == asio::error::eof) {
                if (decoder_.finish()) {
                    report("Discarded incomplete ADC frame at end of stream");
                }
                begin_close("Peer closed the connection", false);
            } else {
                close_now("Read failed", error);
            }
            return;
        }

        bool activity = false;
        std::string_view remaining(read_buffer_.data(), count);
        while (!remaining.empty() && phase_ == Phase::open) {
            auto decoded = decoder_.consume(remaining);
            remaining.remove_prefix(decoded.consumed);
            if (!decoded.frame) {
                continue;
            }
            if (const auto *failure = std::get_if<adc::ParseError>(&*decoded.frame)) {
                handle_parse_error(*failure);
                continue;
            }
            activity = true;
            if (const auto *message = std::get_if<adc::Message>(&*decoded.frame)) {
                handle_message(*message);
            }
        }
        if (phase_ == Phase::open) {
            if (activity && protocol_.state() == adc::SessionState::normal) {
                arm_timer(Clock::now() + limits_.idle_timeout, "Idle timeout", true);
            }
            read_next();
        }
    }

    void handle_parse_error(adc::ParseError error) {
        if (error == adc::ParseError::frame_too_long ||
            error == adc::ParseError::parameter_too_long ||
            error == adc::ParseError::too_many_parameters) {
            close_now("ADC input limit exceeded");
        } else if (error == adc::ParseError::text_processing_failed) {
            close_now("ADC text processing failed");
        } else if (!reported_malformed_) {
            reported_malformed_ = true;
            report("Discarded malformed ADC frame");
        }
    }

    void handle_message(const adc::Message &message) {
        const auto prior = protocol_.state();
        auto result = protocol_.receive(message);
        if (prior == adc::SessionState::protocol &&
            protocol_.state() == adc::SessionState::identify) {
            result.replies.push_back(hub_information());
        }
        if (result.action == adc::SessionAction::identified) {
            if (!protocol_.complete_identification()) {
                close_now("ADC identification could not complete");
                return;
            }
            result.replies.push_back(client_information(message, sid_, peer_address_));
            report("ADC handshake completed");
        }
        if (enqueue(std::move(result.replies)) && result.action == adc::SessionAction::close) {
            begin_close("ADC session closed", false);
        }
    }

    bool enqueue(std::vector<std::string> lines) {
        auto free_bytes = limits_.pending_bytes - queued_bytes_;
        for (const auto &line : lines) {
            if (line.size() > free_bytes) {
                close_now("Pending output limit exceeded");
                return false;
            }
            free_bytes -= line.size();
        }
        for (auto &line : lines) {
            writes_.push_back(std::move(line));
        }
        queued_bytes_ = limits_.pending_bytes - free_bytes;
        write_next();
        return true;
    }

    void write_next() {
        if (write_pending_ || writes_.empty() || phase_ == Phase::closed) {
            return;
        }
        write_pending_ = true;
        // Keep the front buffer alive until this operation completes, including during cancel.
        asio::async_write(stream_, asio::buffer(writes_.front()),
                          [self = this->shared_from_this()](const Error &error, std::size_t) {
                              self->write_pending_ = false;
                              if (self->phase_ == Phase::closed) {
                                  return;
                              }
                              if (error) {
                                  self->close_now("Write failed", error);
                                  return;
                              }
                              self->queued_bytes_ -= self->writes_.front().size();
                              self->writes_.pop_front();
                              self->write_next();
                              self->finish_close();
                          });
    }

    void begin_close(std::string_view reason, bool send_quit) {
        if (phase_ == Phase::establishing) {
            close_now(reason);
            return;
        }
        if (phase_ != Phase::open) {
            return;
        }
        phase_ = Phase::closing;
        close_reason_ = reason;
        arm_timer(Clock::now() + limits_.shutdown_timeout, "Shutdown timeout");
        if (!send_quit || enqueue(protocol_.disconnect().replies)) {
            finish_close();
        }
    }

    void finish_close() {
        if (phase_ != Phase::closing || write_pending_ || !writes_.empty() || shutdown_started_) {
            return;
        }
        if (read_pending_) {
            // Once writes have drained, cancel only the outstanding read before TLS shutdown.
            Error error;
            socket().cancel(error);
            if (error) {
                close_now("Could not cancel the outstanding read", error);
            }
            return;
        }
        if constexpr (secure) {
            shutdown_started_ = true;
            stream_.async_shutdown([self = this->shared_from_this()](const Error &error) {
                if (error && error != asio::error::eof) {
                    self->close_now("TLS shutdown failed", error);
                } else {
                    self->close_now(self->close_reason_);
                }
            });
        } else {
            close_now(close_reason_);
        }
    }

    void close_now(std::string_view reason, const Error &error = {}) {
        if (phase_ == Phase::closed) {
            return;
        }
        phase_ = Phase::closed;
        Error ignored;
        timer_.cancel();
        socket().shutdown(tcp::socket::shutdown_both, ignored);
        socket().close(ignored);
        on_closed_();
        report(reason, error);
    }

    Stream stream_;
    const asio::any_io_executor executor_;
    asio::steady_timer timer_;
    ConnectionLimits limits_;
    Clock::time_point setup_deadline_;
    std::string sid_;
    adc::Session protocol_;
    adc::FrameDecoder decoder_;
    LogSink log_;
    std::function<void()> on_closed_;
    std::optional<asio::ip::address> peer_address_;
    Phase phase_ = Phase::establishing;
    std::string_view close_reason_;
    std::size_t timer_generation_ = 0;
    bool reported_malformed_ = false;
    bool read_pending_ = false;
    bool write_pending_ = false;
    bool shutdown_started_ = false;
    std::array<char, read_chunk_bytes> read_buffer_{};
    std::deque<std::string> writes_;
    std::size_t queued_bytes_ = 0;
};

} // namespace

std::shared_ptr<Connection> make_connection(tcp::socket socket, asio::ssl::context *tls,
                                            ConnectionLimits limits, std::string sid, LogSink log,
                                            std::function<void()> on_closed) {
    if (tls) {
        return std::make_shared<StreamConnection<TlsStream>>(TlsStream(std::move(socket), *tls),
                                                             limits, std::move(sid), std::move(log),
                                                             std::move(on_closed));
    }
    return std::make_shared<StreamConnection<tcp::socket>>(
        std::move(socket), limits, std::move(sid), std::move(log), std::move(on_closed));
}

} // namespace dc24h::net
