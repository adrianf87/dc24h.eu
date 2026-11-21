#include "dc24h/net/server.hpp"

#include "ban_worker.hpp"
#include "connection.hpp"
#include "dc24h/hub/peer_routing.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include <openssl/ssl.h>

#include <atomic>
#include <map>
#include <stdexcept>
#include <utility>

namespace dc24h::net {
namespace {

namespace asio = boost::asio;
using asio::ip::tcp;
using Error = boost::system::error_code;
using Strand = asio::strand<asio::io_context::executor_type>;

constexpr std::uint32_t last_sid = (1U << 20U) - 1U;
constexpr std::size_t max_buffer_bytes = 1024 * 1024;

std::string encode_sid(std::uint32_t value) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string sid(4, 'A');
    for (auto character = sid.rbegin(); character != sid.rend(); ++character) {
        *character = alphabet[value & 31U];
        value >>= 5U;
    }
    return sid;
}

void validate(const ServerConfig &config) {
    if (config.certificate.empty() || config.private_key.empty()) {
        throw std::invalid_argument("ADCS requires a TLS certificate and private key");
    }
    if (config.adc && config.adcs.port != 0 && config.adcs.port == config.adc->port) {
        throw std::invalid_argument("ADC and ADCS require different ports");
    }
    if (config.max_clients == 0 || config.max_clients > last_sid) {
        throw std::invalid_argument("The client limit must be between 1 and 1048575");
    }
    const auto &limits = config.limits;
    if (limits.input_bytes == 0 || limits.pending_bytes == 0 ||
        limits.input_bytes > max_buffer_bytes || limits.pending_bytes > max_buffer_bytes) {
        throw std::invalid_argument("Buffer limits must be between 1 and 1048576 bytes");
    }
    for (const auto timeout : {limits.setup_timeout, limits.tls_timeout, limits.identify_timeout,
                               limits.idle_timeout, limits.shutdown_timeout}) {
        if (timeout <= std::chrono::milliseconds::zero() || timeout > std::chrono::hours(24)) {
            throw std::invalid_argument("Timeouts must be between 1 and 86400000 milliseconds");
        }
    }
    ServerConfig checked;
    if (!config.hub_name.empty()) {
        static_cast<void>(apply_hub_setting(checked, "hub-name", config.hub_name));
    }
    static_cast<void>(apply_hub_setting(checked, "hub-description", config.hub_description));
}

void setup_tls(asio::ssl::context &tls, const ServerConfig &config) {
    auto *context = tls.native_handle();
    constexpr const char *tls12_ciphers =
        "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
        "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
        "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305";
    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1 ||
        SSL_CTX_set_cipher_list(context, tls12_ciphers) != 1) {
        throw std::runtime_error("Could not configure the ADCS TLS policy");
    }
    SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
    // Fail startup for encrypted key files instead of prompting on standard input.
    tls.set_password_callback(
        [](std::size_t, asio::ssl::context::password_purpose) { return std::string{}; });
    tls.use_certificate_chain_file(config.certificate.string());
    tls.use_private_key_file(config.private_key.string(), asio::ssl::context::pem);
    if (SSL_CTX_check_private_key(context) != 1) {
        throw std::runtime_error("The ADCS private key does not match the certificate");
    }
}

struct Listener {
    Listener(const Strand &strand, bool secure) : acceptor(strand), retry(strand), tls(secure) {}

    Endpoint bind(const Endpoint &settings) {
        Error error;
        const auto address = asio::ip::make_address(settings.address, error);
        if (error) {
            throw std::invalid_argument(
                "Listener addresses must be numeric IPv4 or IPv6 addresses");
        }
        const tcp::endpoint requested(address, settings.port);
        acceptor.open(requested.protocol());
        acceptor.set_option(tcp::acceptor::reuse_address(true));
        if (address.is_v6()) {
            acceptor.set_option(asio::ip::v6_only(true));
        }
        acceptor.bind(requested);
        acceptor.listen();
        const auto bound = acceptor.local_endpoint();
        return {bound.address().to_string(), bound.port()};
    }

    void stop() {
        Error error;
        retry.cancel();
        acceptor.close(error);
    }

    tcp::acceptor acceptor;
    asio::steady_timer retry;
    bool tls;
};

} // namespace

class Server::Impl : public std::enable_shared_from_this<Server::Impl> {
  public:
    Impl(asio::io_context &io, ServerConfig config, LogSink log, std::shared_ptr<BanWorker> bans)
        : io_(io), control_(asio::make_strand(io)), config_(std::move(config)),
          log_(std::move(log)), bans_(std::move(bans)), tls_(asio::ssl::context::tls_server),
          adcs_(control_, true), presence_(config_.max_nick_codepoints, config_.max_message_bytes,
                                           config_.limits.input_bytes),
          peers_(presence_, [this](std::string_view sid, const hub::WireFrame &frame) {
              return send(sid, frame);
          }) {
        validate(config_);
        setup_tls(tls_, config_);
        adcs_endpoint_ = adcs_.bind(config_.adcs);
        if (config_.adc) {
            adc_.emplace(control_, false);
            adc_endpoint_ = adc_->bind(*config_.adc);
            if (adc_endpoint_->port == adcs_endpoint_.port) {
                throw std::invalid_argument("ADC and ADCS require different ports");
            }
        }
    }

    void start() {
        asio::post(control_, [self = shared_from_this()] {
            self->accept(self->adcs_);
            if (self->adc_) {
                self->accept(*self->adc_);
            }
        });
    }

    void stop() {
        // Accept handlers can stop scheduling work as soon as they see the flag.
        // Keep run() alive until the listener shutdown has been posted.
        const auto work = asio::make_work_guard(io_);
        if (stop_requested_.exchange(true)) {
            return;
        }
        asio::post(control_, [self = shared_from_this()] {
            self->adcs_.stop();
            if (self->adc_) {
                self->adc_->stop();
            }
            for (const auto &entry : self->connections_) {
                entry.second->stop();
            }
        });
    }

    [[nodiscard]] const Endpoint &adcs_endpoint() const noexcept { return adcs_endpoint_; }

    [[nodiscard]] const std::optional<Endpoint> &adc_endpoint() const noexcept {
        return adc_endpoint_;
    }

    [[nodiscard]] std::size_t active_clients() const noexcept {
        return active_clients_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t online_users() const noexcept {
        return online_users_.load(std::memory_order_relaxed);
    }

    bool read_users(std::string after, std::size_t limit, UsersHandler reply) {
        if (stop_requested_.load() || io_.stopped()) {
            return false;
        }
        asio::post(control_, [self = shared_from_this(), after = std::move(after), limit,
                              reply = std::move(reply)] {
            if (!self->stop_requested_.load()) {
                reply(self->presence_.user_page(after, limit));
            }
        });
        return true;
    }

  private:
    void accept(Listener &listener) {
        if (stop_requested_.load()) {
            return;
        }
        auto socket = std::make_shared<tcp::socket>(asio::make_strand(io_));
        listener.acceptor.async_accept(
            *socket, [self = shared_from_this(), &listener, socket](const Error &error) {
                if (self->stop_requested_.load()) {
                    return;
                }
                if (error) {
                    self->retry_accept(listener, error);
                    return;
                }
                self->adopt(std::move(*socket), listener.tls);
                self->accept(listener);
            });
    }

    void retry_accept(Listener &listener, const Error &error) {
        if (log_) {
            log_("TCP accept failed: " + error.message());
        }
        listener.retry.expires_after(std::chrono::milliseconds(100));
        listener.retry.async_wait([self = shared_from_this(), &listener](const Error &result) {
            if (!result) {
                self->accept(listener);
            }
        });
    }

    void adopt(tcp::socket socket, bool secure) {
        if (connections_.size() >= config_.max_clients) {
            Error ignored;
            socket.close(ignored);
            if (!reported_full_ && log_) {
                log_("Connection refused at the client limit");
                reported_full_ = true;
            }
            return;
        }

        // SID zero is reserved. A slot remains occupied until its socket has closed.
        std::string sid;
        do {
            sid = encode_sid(next_sid_);
            next_sid_ = next_sid_ == last_sid ? 1U : next_sid_ + 1U;
        } while (connections_.contains(sid));
        auto input = [weak = weak_from_this(), sid](std::optional<Identity> identity,
                                                    adc::Message message) {
            if (auto owner = weak.lock()) {
                const auto executor = owner->control_;
                asio::post(executor, [owner = std::move(owner), sid, identity = std::move(identity),
                                      message = std::move(message)]() mutable {
                    owner->process_input(sid, std::move(identity), std::move(message));
                });
            }
        };
        auto released = [weak = weak_from_this(), sid] {
            if (auto owner = weak.lock()) {
                const auto executor = owner->control_;
                asio::post(executor, [owner = std::move(owner), sid] { owner->release(sid); });
            }
        };
        auto connection = make_connection(std::move(socket), secure ? &tls_ : nullptr, config_, sid,
                                          log_, std::move(input), std::move(released));
        connections_.emplace(sid, connection);
        active_clients_.store(connections_.size(), std::memory_order_relaxed);
        connection->start();
    }

    bool send(std::string_view sid, const hub::WireFrame &frame) {
        const auto found = connections_.find(sid);
        return found != connections_.end() && found->second->deliver({frame});
    }

    void broadcast(const hub::WireFrame &frame, std::string_view except = {}) {
        for (const auto &entry : presence_.roster()) {
            if (entry.sid != except) {
                send(entry.sid, frame);
            }
        }
    }

    void release(const std::string &sid) {
        if (connections_.erase(sid) == 0) {
            return;
        }
        active_clients_.store(connections_.size(), std::memory_order_relaxed);
        reported_full_ = false;
        if (presence_.remove(sid)) {
            peers_.forget(sid);
            online_users_.store(presence_.size(), std::memory_order_relaxed);
            if (!stop_requested_.load()) {
                broadcast(std::make_shared<const std::string>("IQUI " + sid + '\n'));
            }
        }
    }

    void admit(const std::string &sid, Connection &connection, const adc::Message &message) {
        auto result = presence_.admit(sid, message);
        if (const auto *error = std::get_if<hub::Rejection>(&result)) {
            static_cast<void>(connection.deliver({error->reply(true)}, Continuation::reject));
            return;
        }

        const auto &own = std::get<hub::WireFrame>(result);
        const auto roster = presence_.roster();
        std::vector<hub::WireFrame> frames;
        frames.reserve(roster.size());
        for (const auto &entry : roster) {
            if (entry.sid != sid) {
                frames.push_back(entry.information);
            }
        }
        // ADC finishes NORMAL entry with the joining client's own BINF.
        frames.push_back(own);
        if (!connection.deliver(std::move(frames), Continuation::admit)) {
            static_cast<void>(presence_.remove(sid));
            return;
        }
        online_users_.store(presence_.size(), std::memory_order_relaxed);
        for (const auto &entry : roster) {
            if (entry.sid != sid) {
                send(entry.sid, own);
            }
        }
    }

    void finish_admission(const std::string &sid, Connection &connection, const adc::Message &info,
                          const BanDecision &decision) {
        if (database_available_ != decision.available) {
            database_available_ = decision.available;
            if (log_) {
                log_(decision.available ? "MariaDB admission checks recovered"
                                        : "MariaDB admission checks unavailable");
            }
        }
        std::string reply;
        if (!decision.available) {
            reply = adc::format_status("210", "Admission temporarily unavailable");
        } else if (!decision.ban) {
            admit(sid, connection, info);
            return;
        } else if (const auto remaining = decision.ban->seconds_remaining) {
            reply =
                adc::format_status("232", "Temporarily banned", "TL" + std::to_string(*remaining));
        } else {
            reply = adc::format_status("231", "Permanently banned");
        }
        static_cast<void>(connection.deliver(
            {std::make_shared<const std::string>(std::move(reply))}, Continuation::reject));
    }

    void check_admission(const std::string &sid, const std::shared_ptr<Connection> &connection,
                         Identity identity, adc::Message info) {
        auto complete = [weak = weak_from_this(), client = std::weak_ptr(connection),
                         executor = control_, sid,
                         info = std::move(info)](BanDecision decision) mutable {
            asio::post(executor, [weak, client, sid, info = std::move(info),
                                  decision = std::move(decision)] {
                if (const auto owner = weak.lock(); owner && !owner->stop_requested_.load()) {
                    const auto found = owner->connections_.find(sid);
                    if (found != owner->connections_.end() && found->second == client.lock()) {
                        owner->finish_admission(sid, *found->second, info, decision);
                    }
                }
            });
        };
        if (!bans_->submit(std::move(identity.cid), std::move(identity.ip), std::move(complete))) {
            finish_admission(sid, *connection, {}, {});
        }
    }

    hub::WireFrame update(std::string_view sid, const adc::Message &message) {
        const auto result = presence_.update(sid, message);
        if (const auto *error = std::get_if<hub::Rejection>(&result)) {
            return error->reply();
        }
        if (const auto &frame = std::get<hub::WireFrame>(result)) {
            broadcast(frame);
        }
        return {};
    }

    hub::WireFrame route_chat(std::string_view sid, const adc::Message &message) {
        const auto result = presence_.chat(sid, message);
        if (const auto *error = std::get_if<hub::Rejection>(&result)) {
            return error->reply();
        }
        const auto &delivery = std::get<hub::ChatDelivery>(result);
        for (const auto recipient : delivery.recipients) {
            if (!send(recipient, delivery.frame) && delivery.directed) {
                // A directed message must be accepted by the target before echoing it.
                return hub::Rejection{"25", "Recipient is unavailable", "FC" + message.fourcc()}
                    .reply();
            }
        }
        return {};
    }

    void process_input(const std::string &sid, std::optional<Identity> identity,
                       adc::Message message) {
        const auto found = connections_.find(sid);
        if (found == connections_.end() || stop_requested_.load()) {
            return;
        }
        auto &connection = *found->second;
        if (identity) {
            if (bans_) {
                check_admission(sid, found->second, std::move(*identity), std::move(message));
            } else {
                admit(sid, connection, message);
            }
            return;
        }
        hub::WireFrame reply;
        if (message.command == "INF") {
            reply = update(sid, message);
        } else if (message.command == "MSG") {
            reply = route_chat(sid, message);
        } else if (const auto error = peers_.route(sid, message)) {
            reply = error->reply();
        }
        std::vector<hub::WireFrame> responses;
        if (reply) {
            responses.push_back(std::move(reply));
        }
        static_cast<void>(connection.deliver(std::move(responses), Continuation::resume));
    }

    asio::io_context &io_;
    Strand control_;
    ServerConfig config_;
    LogSink log_;
    std::shared_ptr<BanWorker> bans_;
    asio::ssl::context tls_;
    Listener adcs_;
    std::optional<Listener> adc_;
    Endpoint adcs_endpoint_;
    std::optional<Endpoint> adc_endpoint_;
    hub::Presence presence_;
    hub::PeerRouting peers_;
    std::map<std::string, std::shared_ptr<Connection>, std::less<>> connections_;
    std::atomic<std::size_t> active_clients_{0};
    std::atomic<std::size_t> online_users_{0};
    std::atomic<bool> stop_requested_{false};
    std::uint32_t next_sid_ = 1;
    bool reported_full_ = false;
    bool database_available_ = true;
};

Server::Server(asio::io_context &io, ServerConfig config, LogSink log,
               std::optional<db::Config> database)
    : bans_(database ? std::make_shared<BanWorker>(std::move(*database)) : nullptr),
      impl_(std::make_shared<Impl>(io, std::move(config), std::move(log), bans_)) {
    impl_->start();
}

Server::~Server() {
    stop();
    if (bans_) {
        bans_->join();
    }
}

const Endpoint &Server::adcs_endpoint() const noexcept { return impl_->adcs_endpoint(); }

const std::optional<Endpoint> &Server::adc_endpoint() const noexcept {
    return impl_->adc_endpoint();
}

std::size_t Server::active_clients() const noexcept { return impl_->active_clients(); }

std::size_t Server::online_users() const noexcept { return impl_->online_users(); }

bool Server::read_users(std::string after, std::size_t limit, UsersHandler reply) const {
    return impl_->read_users(std::move(after), limit, std::move(reply));
}

void Server::stop() {
    impl_->stop();
    if (bans_) {
        bans_->stop();
    }
}

} // namespace dc24h::net
