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
#include <atomic>
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

std::string hub_information(const ServerConfig &config) {
    std::string line = "IINF CT32 NI";
    line += config.hub_name.empty() ? std::string(project_name)
                                    : adc::escape_parameter(config.hub_name);
    line += " VE";
    line += version;
    if (!config.hub_description.empty()) {
        line += " DE";
        line += adc::escape_parameter(config.hub_description);
    }
    line += '\n';
    return line;
}

void prepare_info(adc::Message &message, const std::optional<asio::ip::address> &peer) {
    // Keep private identity data on the socket strand, where E1 validates it.
    std::erase_if(message.parameters, [&](const std::string &field) {
        const auto name = std::string_view(field).substr(0, 2);
        return name == "PD" || name == "EM" || name == "RF" || name == "TO" || name == "CT" ||
               (name == "I4" && (!peer || !peer->is_v4())) ||
               (name == "I6" && (!peer || !peer->is_v6()));
    });
    if (!peer) {
        return;
    }
    for (auto &field : message.parameters) {
        if (field.size() > 2 && (field.starts_with("I4") || field.starts_with("I6"))) {
            field.resize(2);
            field += peer->to_string();
        }
    }
}

template <typename Stream>
class StreamConnection final : public Connection,
                               public std::enable_shared_from_this<StreamConnection<Stream>> {
  public:
    StreamConnection(Stream stream, const ServerConfig &config, std::string sid, LogSink log,
                     InputHandler on_input, std::function<void()> on_closed)
        : stream_(std::move(stream)), executor_(stream_.get_executor()), timer_(executor_),
          limits_(config.limits), setup_deadline_(Clock::now() + limits_.setup_timeout),
          sid_(std::move(sid)), protocol_(sid_), decoder_({.frame_bytes = limits_.input_bytes}),
          hub_info_(hub_information(config)), log_(std::move(log)), on_input_(std::move(on_input)),
          on_closed_(std::move(on_closed)) {}

    void start() override {
        asio::post(executor_, [self = this->shared_from_this()] { self->establish(); });
    }

    void stop() override {
        asio::post(executor_, [self = this->shared_from_this()] {
            self->begin_close("Server shutdown", true);
        });
    }

    bool deliver(std::vector<hub::WireFrame> frames, Continuation after) override {
        if (!can_deliver_.load()) {
            return false;
        }
        const auto bytes = reserve_output(frames);
        if (!bytes) {
            if (can_deliver_.exchange(false)) {
                asio::post(executor_, [self = this->shared_from_this()] {
                    self->close_now("Pending output limit exceeded");
                });
            }
            return false;
        }
        try {
            asio::post(executor_, [self = this->shared_from_this(), frames = std::move(frames),
                                   after, reserved = *bytes]() mutable {
                if (self->phase_ != Phase::open) {
                    self->reserved_bytes_.fetch_sub(reserved, std::memory_order_relaxed);
                    return;
                }
                self->enqueue_reserved(std::move(frames));
                self->continue_input(after);
            });
        } catch (...) {
            reserved_bytes_.fetch_sub(*bytes, std::memory_order_relaxed);
            throw;
        }
        return true;
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

        unread_ = std::string_view(read_buffer_.data(), count);
        pump_input();
    }

    void pump_input() {
        bool activity = false;
        while (!unread_.empty() && phase_ == Phase::open && !waiting_for_hub_) {
            auto decoded = decoder_.consume(unread_);
            unread_.remove_prefix(decoded.consumed);
            if (!decoded.frame) {
                continue;
            }
            if (const auto *failure = std::get_if<adc::ParseError>(&*decoded.frame)) {
                handle_parse_error(*failure);
                continue;
            }
            activity = true;
            if (auto *message = std::get_if<adc::Message>(&*decoded.frame)) {
                handle_message(*message);
            }
        }
        if (phase_ == Phase::open) {
            if (activity && protocol_.state() == adc::SessionState::normal) {
                arm_timer(Clock::now() + limits_.idle_timeout, "Idle timeout", true);
            }
            if (!waiting_for_hub_) {
                read_next();
            }
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

    void handle_message(adc::Message &message) {
        const auto prior = protocol_.state();
        auto result = protocol_.receive(message);
        if (prior == adc::SessionState::protocol &&
            protocol_.state() == adc::SessionState::identify) {
            result.replies.push_back(std::move(hub_info_));
        }
        if (!enqueue(std::move(result.replies))) {
            return;
        }
        if (result.action == adc::SessionAction::close) {
            begin_close("ADC session closed", false);
            return;
        }
        const bool identifying = result.action == adc::SessionAction::identified;
        if (identifying || (result.action == adc::SessionAction::dispatch &&
                            (message.command == "INF" || message.command == "MSG" ||
                             adc::is_peer_command(message.command)))) {
            if (message.command == "INF") {
                prepare_info(message, peer_address_);
            }
            // Leave the remainder of this read in the fixed buffer until the hub replies.
            waiting_for_hub_ = true;
            std::optional<Identity> identity;
            if (identifying) {
                identity = Identity{std::string(protocol_.cid()),
                                    peer_address_ ? peer_address_->to_string() : std::string{}};
            }
            on_input_(std::move(identity), std::move(message));
        }
    }

    void continue_input(Continuation after) {
        if (after == Continuation::none) {
            return;
        }
        if (after == Continuation::reject) {
            begin_close("ADC identification rejected", false);
            return;
        }
        if (after == Continuation::admit) {
            if (!protocol_.complete_identification()) {
                close_now("ADC identification could not complete");
                return;
            }
            report("ADC handshake completed");
            arm_timer(Clock::now() + limits_.idle_timeout, "Idle timeout", true);
        }
        waiting_for_hub_ = false;
        pump_input();
    }

    std::optional<std::size_t> reserve_output(const std::vector<hub::WireFrame> &frames) {
        std::size_t bytes = 0;
        for (const auto &frame : frames) {
            if (frame->size() > limits_.pending_bytes - bytes) {
                return std::nullopt;
            }
            bytes += frame->size();
        }
        if (bytes == 0) {
            return 0;
        }
        // Count posted batches, queued frames, and the active write against the same limit.
        auto used = reserved_bytes_.load(std::memory_order_relaxed);
        do {
            if (bytes > limits_.pending_bytes - used) {
                return std::nullopt;
            }
        } while (
            !reserved_bytes_.compare_exchange_weak(used, used + bytes, std::memory_order_relaxed));
        return bytes;
    }

    bool enqueue(std::vector<std::string> lines) {
        std::vector<hub::WireFrame> frames;
        frames.reserve(lines.size());
        for (auto &line : lines) {
            frames.push_back(std::make_shared<const std::string>(std::move(line)));
        }
        if (!reserve_output(frames)) {
            close_now("Pending output limit exceeded");
            return false;
        }
        enqueue_reserved(std::move(frames));
        return true;
    }

    void enqueue_reserved(std::vector<hub::WireFrame> frames) {
        for (auto &frame : frames) {
            writes_.push_back(std::move(frame));
        }
        write_next();
    }

    void write_next() {
        if (write_pending_ || writes_.empty() || phase_ == Phase::closed) {
            return;
        }
        write_pending_ = true;
        // Keep the front buffer alive until this operation completes, including during cancel.
        asio::async_write(stream_, asio::buffer(*writes_.front()),
                          [self = this->shared_from_this()](const Error &error, std::size_t) {
                              self->write_pending_ = false;
                              if (self->phase_ == Phase::closed) {
                                  return;
                              }
                              if (error) {
                                  self->close_now("Write failed", error);
                                  return;
                              }
                              self->reserved_bytes_.fetch_sub(self->writes_.front()->size(),
                                                              std::memory_order_relaxed);
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
        can_deliver_.store(false);
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
        can_deliver_.store(false);
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
    std::string hub_info_;
    LogSink log_;
    InputHandler on_input_;
    std::function<void()> on_closed_;
    std::optional<asio::ip::address> peer_address_;
    Phase phase_ = Phase::establishing;
    std::string_view close_reason_;
    std::size_t timer_generation_ = 0;
    bool reported_malformed_ = false;
    bool read_pending_ = false;
    bool write_pending_ = false;
    bool shutdown_started_ = false;
    bool waiting_for_hub_ = false;
    std::array<char, read_chunk_bytes> read_buffer_{};
    std::string_view unread_;
    std::deque<hub::WireFrame> writes_;
    std::atomic<std::size_t> reserved_bytes_{0};
    std::atomic<bool> can_deliver_{true};
};

} // namespace

std::shared_ptr<Connection> make_connection(tcp::socket socket, asio::ssl::context *tls,
                                            const ServerConfig &config, std::string sid,
                                            LogSink log, InputHandler on_input,
                                            std::function<void()> on_closed) {
    if (tls) {
        return std::make_shared<StreamConnection<TlsStream>>(
            TlsStream(std::move(socket), *tls), config, std::move(sid), std::move(log),
            std::move(on_input), std::move(on_closed));
    }
    return std::make_shared<StreamConnection<tcp::socket>>(
        std::move(socket), config, std::move(sid), std::move(log), std::move(on_input),
        std::move(on_closed));
}

} // namespace dc24h::net
