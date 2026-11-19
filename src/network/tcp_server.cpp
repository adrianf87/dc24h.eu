#include "dc24h/network/tcp_server.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/adc/parser.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adcs/tls.hpp"

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <memory>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace dc24h::network {
namespace {
using Clock = std::chrono::steady_clock;
using Event = logging::ConnectionEvent;
using Cause = users::CloseCause;
constexpr std::array close_events{
    std::pair{Cause::protocol_rejected,Event::login_rejected},
    std::pair{Cause::peer_status,Event::peer_status}, std::pair{Cause::peer_closed,Event::peer_closed},
    std::pair{Cause::tls_handshake_failed,Event::tls_handshake_failed}, std::pair{Cause::tls_closed,Event::tls_closed},
    std::pair{Cause::tls_read_error,Event::tls_read_error}, std::pair{Cause::tcp_read_error,Event::tcp_read_error},
    std::pair{Cause::socket_error,Event::socket_error}, std::pair{Cause::write_error,Event::write_error},
    std::pair{Cause::shutdown,Event::shutdown}, std::pair{Cause::login_timeout,Event::login_timeout},
    std::pair{Cause::idle_timeout,Event::idle_timeout}, std::pair{Cause::handshake_timeout,Event::handshake_timeout},
    std::pair{Cause::partial_frame_timeout,Event::partial_frame_timeout}, std::pair{Cause::output_timeout,Event::output_timeout},
    std::pair{Cause::output_limit,Event::output_limit}, std::pair{Cause::security_quota,Event::security_quota},
    std::pair{Cause::negotiation_limit,Event::negotiation_limit}, std::pair{Cause::identity_limit,Event::identity_limit},
    std::pair{Cause::authentication_revoked,Event::authentication_revoked},
    std::pair{Cause::kicked,Event::kicked},
    std::pair{Cause::banned,Event::banned},
    std::pair{Cause::authentication_limit,Event::authentication_limit},
    std::pair{Cause::close_timeout,Event::close_timeout}, std::pair{Cause::internal_error,Event::socket_error}
};
Cause session_cause(Event event) noexcept {
    for (const auto& [cause,value] : close_events) { if (value==event) { return cause; } }
    return Cause::internal_error;
}
Event session_event(std::optional<Cause> cause) noexcept {
    for (const auto& [value,event] : close_events) { if (cause==value) { return event; } }
    return Event::login_rejected;
}

class Fd final {
public:
    explicit Fd(int value = -1) noexcept : value_(value) {}
    ~Fd() { if (value_ >= 0) { ::close(value_); } }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) {
            if (value_ >= 0) { ::close(value_); }
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    [[nodiscard]] int get() const noexcept { return value_; }
private:
    int value_;
};

[[noreturn]] void system_failure(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}

sockaddr_storage socket_address(const Endpoint& endpoint, socklen_t& size) {
    if (endpoint.address.find('\0') != std::string::npos) {
        throw std::invalid_argument("Listener address must not contain NUL bytes");
    }
    sockaddr_storage storage{};
    auto* ipv4 = reinterpret_cast<sockaddr_in*>(&storage);
    if (::inet_pton(AF_INET, endpoint.address.c_str(), &ipv4->sin_addr) == 1) {
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons(endpoint.port);
        size = sizeof(sockaddr_in);
        return storage;
    }
    auto* ipv6 = reinterpret_cast<sockaddr_in6*>(&storage);
    if (::inet_pton(AF_INET6, endpoint.address.c_str(), &ipv6->sin6_addr) == 1) {
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons(endpoint.port);
        size = sizeof(sockaddr_in6);
        return storage;
    }
    throw std::invalid_argument("Listener address must be a numeric IPv4 or IPv6 address");
}

std::string bound_address(int fd) {
    sockaddr_storage storage{};
    socklen_t size = sizeof(storage);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&storage), &size) == -1) {
        system_failure("getsockname");
    }
    std::array<char, INET6_ADDRSTRLEN> text{};
    const bool ipv6 = storage.ss_family == AF_INET6;
    const auto* v4 = reinterpret_cast<const sockaddr_in*>(&storage);
    const auto* v6 = reinterpret_cast<const sockaddr_in6*>(&storage);
    const void* address = ipv6 ? static_cast<const void*>(&v6->sin6_addr)
                               : static_cast<const void*>(&v4->sin_addr);
    if (::inet_ntop(storage.ss_family, address, text.data(), text.size()) == nullptr) {
        system_failure("inet_ntop");
    }
    const auto port = ntohs(ipv6 ? v6->sin6_port : v4->sin_port);
    return (ipv6 ? "[" + std::string(text.data()) + "]" : std::string(text.data())) +
           ":" + std::to_string(port);
}

void watch(int epoll_fd, int fd, std::uint64_t token, std::uint32_t flags) {
    epoll_event event{};
    event.events = flags;
    event.data.u64 = token;
    if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        system_failure("epoll_ctl ADD");
    }
}

void update_client_interest(int epoll_fd, int fd, std::uint64_t token, bool want_read, bool want_write) {
    epoll_event event{};
    event.events = (want_read ? EPOLLIN : 0U) | (want_write ? EPOLLOUT : 0U) | EPOLLRDHUP;
    event.data.u64 = token;
    if (::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1) {
        system_failure("epoll_ctl MOD");
    }
}

// Canonical identities are used only by the in-memory admission limiter.
std::string peer_identity(const sockaddr_storage& address) {
    std::array<char, INET6_ADDRSTRLEN> text{};
    int family = address.ss_family;
    const void* bytes = nullptr;
    if (family == AF_INET) {
        bytes = &reinterpret_cast<const sockaddr_in*>(&address)->sin_addr;
    } else if (family == AF_INET6) {
        const auto& ipv6 = reinterpret_cast<const sockaddr_in6*>(&address)->sin6_addr;
        if (IN6_IS_ADDR_V4MAPPED(&ipv6)) { family = AF_INET; bytes = &ipv6.s6_addr[12]; }
        else { bytes = &ipv6; }
    } else { throw std::runtime_error("Unsupported peer address family"); }
    if (::inet_ntop(family, bytes, text.data(), text.size()) == nullptr) { system_failure("inet_ntop peer"); }
    return std::string(family == AF_INET ? "4:" : "6:") + text.data();
}

struct Connection {
    Fd fd;
    Clock::time_point last_activity;
    adc::StreamDecoder decoder;
    std::unique_ptr<adcs::Connection> tls;
    bool tls_ready{true};
    bool want_write{false};
    std::string peer_key;
    std::optional<Clock::time_point> partial_started;
    std::size_t invalid_frames{0};
    security::FrameLimiter frame_budget{1024};
    bool security_violation{false};
    std::optional<adcs::IoStatus> write_wait;
    std::array<char, read_buffer_bytes> input{};
    std::optional<Clock::time_point> output_progress;
    std::uint32_t interest{EPOLLIN | EPOLLRDHUP};
    bool security_counted{false};
    security::NegotiationBudget negotiation{65536,64};
    bool negotiation_violation{false};
    bool identity_violation{false};
};
} // namespace

bool protect_tcp_socket(int fd, std::chrono::milliseconds user_timeout) noexcept {
    if (fd < 0) { return false; }
    const auto milliseconds = user_timeout.count();
    if (milliseconds < 1 || milliseconds > 86400000) { return false; }
    const int buffer = socket_buffer_bytes;
    if (::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffer, sizeof(buffer)) != 0) { return false; }
    if (::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer)) != 0) { return false; }
    const auto timeout = static_cast<unsigned int>(milliseconds);
    return ::setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout)) == 0;
}

Endpoint parse_endpoint(std::string_view text) {
    const bool bracketed = text.starts_with('[');
    const auto separator = bracketed ? text.find("]:") : text.find(':');
    if (separator == std::string_view::npos || separator == 0 ||
        (bracketed && separator == 1)) {
        throw std::invalid_argument("Expected IPv4:port or [IPv6]:port");
    }
    const auto address = bracketed ? text.substr(1, separator - 1) : text.substr(0, separator);
    const auto port_text = text.substr(separator + (bracketed ? 2 : 1));
    unsigned int port{};
    const auto [end, error] = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port);
    if (error != std::errc{} || end != port_text.data() + port_text.size() || port > 65535) {
        throw std::invalid_argument("Listener port must be an integer from 0 to 65535");
    }
    Endpoint endpoint{std::string(address), static_cast<std::uint16_t>(port)};
    socklen_t size{};
    const auto storage = socket_address(endpoint, size);
    if ((storage.ss_family == AF_INET6) != bracketed) {
        throw std::invalid_argument("Use brackets only for IPv6 listeners");
    }
    return endpoint;
}

TcpServer::TcpServer(logging::Logger& logger, Options options)
    : logger_(logger), options_(std::move(options)) {
    if ((options_.listeners.empty() && options_.tls_listeners.empty()) ||
        options_.listeners.size() + options_.tls_listeners.size() > 16 ||
        options_.max_connections == 0 || options_.max_connections > 65535 ||
        options_.idle_timeout < std::chrono::seconds(1) ||
        options_.idle_timeout > std::chrono::hours(24)) {
        throw std::invalid_argument("Invalid TCP limits (listeners 1..16, connections 1..65535, idle 1..86400s)");
    }
    if (options_.tls_listeners.empty() ?
        (!options_.tls_certificate.empty() || !options_.tls_private_key.empty()) :
        (options_.tls_certificate.empty() || options_.tls_private_key.empty())) {
        throw std::invalid_argument("ADCS needs both TLS certificate and private key with a TLS listener");
    }
    security::validate(options_.security);
    security::validate(options_.authentication_limits);
    security::validate(options_.roles);
    chat::validate_settings(options_.chat);
    search::validate_security(options_.search);
    transfers::validate_limits(options_.transfers.flood);
    users::validate(options_.users);
    for (const auto& endpoint : options_.listeners) {
        socklen_t size{};
        (void)socket_address(endpoint, size);
    }
    for (const auto& endpoint : options_.tls_listeners) {
        socklen_t size{};
        (void)socket_address(endpoint, size);
    }
    wake_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wake_fd_ == -1) { system_failure("eventfd"); }
}

TcpServer::~TcpServer() { ::close(wake_fd_); }

void TcpServer::request_stop() noexcept {
    stop_.store(true);
    const std::uint64_t value = 1;
    // EAGAIN means a wakeup is already queued. The eventfd stays valid until join.
    while (::write(wake_fd_, &value, sizeof(value)) == -1 && errno == EINTR) {}
}

Statistics TcpServer::statistics() const noexcept {
    Statistics result{active_.load(), accepted_.load(), rejected_.load(), received_bytes_.load(),
            adc_frames_.load(), adc_unknown_.load(), adc_invalid_.load(), adc_keepalives_.load(),
            adcs_handshakes_.load(), adcs_incomplete_.load(),
            security_rejected_.load(), security_closed_.load(), closed_.load(), sent_bytes_.load(),
            peak_connections_.load(), online_users_.load(), peak_online_users_.load(), successful_logins_.load()};
    for (std::size_t i = 0; i < chat_statistics_.size(); ++i) { result.chat.values[i] = chat_statistics_[i].load(); }
    for(std::size_t i=0;i<search_statistics_.size();++i){result.search.values[i]=search_statistics_[i].load();}
    result.search.tracked_searches=tracked_searches_.load();
    for(std::size_t i=0;i<transfer_statistics_.size();++i){result.transfers.values[i]=transfer_statistics_[i].load();}
    for(std::size_t i=0;i<kick_statistics_.size();++i){result.kick.values[i]=kick_statistics_[i].load();}
    for(std::size_t i=0;i<mute_statistics_.size();++i){result.mute.values[i]=mute_statistics_[i].load();}
    return result;
}

users::PresenceDirectory TcpServer::presence() const {
    std::lock_guard lock(presence_mu_);
    return presence_;
}

void TcpServer::run() {
    if (used_.exchange(true)) { throw std::logic_error("TcpServer::run() may only be called once"); }
    if (stop_.load()) { return; }

    // Reset active on all exits, including exceptions after partially completed startup.
    struct ResetActive {
        std::atomic<std::size_t>& value;
        std::atomic<std::uint64_t>& closed;
        std::atomic<std::size_t>& online;
        ~ResetActive() {
            // RAII closes all remaining sockets on exceptional exits too.
            closed.fetch_add(value.exchange(0));
            online.store(0);
        }
    } reset{active_, closed_, online_users_};
    Fd epoll(::epoll_create1(EPOLL_CLOEXEC));
    if (epoll.get() == -1) { system_failure("epoll_create1"); }
    watch(epoll.get(), wake_fd_, 0, EPOLLIN);
    std::unique_ptr<adcs::Context> tls_context;
    if (!options_.tls_listeners.empty()) {
        tls_context = std::make_unique<adcs::Context>(options_.tls_certificate, options_.tls_private_key);
    }
    std::vector<Fd> listeners;
    std::vector<bool> secure_listeners;
    std::vector<std::string> addresses;
    auto add_listener = [&](const Endpoint& endpoint, bool secure) {
        if (stop_.load()) { return; }
        socklen_t size{};
        const auto storage = socket_address(endpoint, size);
        Fd listener(::socket(storage.ss_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        if (listener.get() == -1) { system_failure("socket"); }
        const int enabled = 1;
        if (::setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) == -1) {
            system_failure("setsockopt SO_REUSEADDR");
        }
        // Separate IPv4/IPv6 listeners; never silently expose an IPv4 wildcard via IPv6.
        if (storage.ss_family == AF_INET6 &&
            ::setsockopt(listener.get(), IPPROTO_IPV6, IPV6_V6ONLY, &enabled, sizeof(enabled)) == -1) {
            system_failure("setsockopt IPV6_V6ONLY");
        }
        if (::bind(listener.get(), reinterpret_cast<const sockaddr*>(&storage), size) == -1) {
            system_failure("bind TCP listener");
        }
        if (::listen(listener.get(), listen_backlog) == -1) { system_failure("listen"); }
        watch(epoll.get(), listener.get(), listeners.size() + 1, EPOLLIN);
        addresses.push_back(std::string(secure ? "ADCS" : "Network") +
                            " listening on " + bound_address(listener.get()));
        listeners.push_back(std::move(listener));
        secure_listeners.push_back(secure);
    };
    for (const auto& endpoint : options_.listeners) { add_listener(endpoint, false); }
    for (const auto& endpoint : options_.tls_listeners) { add_listener(endpoint, true); }
    // Log readiness only after every configured listener has started successfully.
    for (const auto& address : addresses) { logger_.info(address); }
    std::unordered_map<std::uint64_t, Connection> connections;
    adc::LoginLimits login_limits;
    login_limits.anti_flood = options_.chat.flood;
    login_limits.chat = options_.chat.policy;
    login_limits.search = options_.search;
    login_limits.transfers = options_.transfers;
    login_limits.welcome_text = options_.chat.welcome_enabled ? std::optional<std::string>(options_.chat.welcome_message) : std::nullopt;
    login_limits.connections=options_.max_connections;
    login_limits.users=options_.users;
    login_limits.authentication=options_.authentication;
    login_limits.authentication_limits=options_.authentication_limits;
    login_limits.roles=options_.roles;login_limits.bans=options_.bans;
    adc::LoginHub login(login_limits);
    auto publish_users = [&]() noexcept {
        const auto value = login.sessions().statistics();
        online_users_.store(value.online());
        peak_online_users_.store(value.peak_online);
        successful_logins_.store(value.successful_logins);
        const auto search=login.search_statistics();
        for(std::size_t i=0;i<search_statistics_.size();++i){search_statistics_[i].store(search.values[i]);}
        tracked_searches_.store(search.tracked_searches);
        const auto transfers = login.transfer_statistics();
        for(std::size_t i=0;i<transfer_statistics_.size();++i){transfer_statistics_[i].store(transfers.values[i]);}
        const auto mute=login.mute_statistics();
        for(std::size_t i=0;i<mute_statistics_.size();++i){mute_statistics_[i].store(mute.values[i]);}
        const auto kick=login.kick_statistics();
        for(std::size_t i=0;i<kick_statistics_.size();++i){kick_statistics_[i].store(kick.values[i]);}
        const auto chat = login.chat_statistics();
        for (std::size_t i = 0; i < chat_statistics_.size(); ++i) { chat_statistics_[i].store(chat.values[i]); }
        users::PresenceDirectory directory;
        login.sessions().export_presence(directory);
        if (presence_mu_.try_lock()) {
            presence_ = directory;
            presence_mu_.unlock();
        }
    };
    // Publish the final committed model totals before LoginHub destruction,
    // including an exceptional reactor exit. Never expose models to API threads.
    struct PublishOnExit {
        decltype(publish_users)& publish;
        ~PublishOnExit() { publish(); }
    } publish_on_exit{publish_users};
    security::PeerLimiter peer_limiter(options_.security);
    users::SessionIds session_ids(static_cast<std::uint64_t>(listeners.size()+1));
    std::array<epoll_event, 64> events{};
    bool accepting = true;
    Clock::time_point resume_accepting{};

    enum class Warning { invalid_frame, connection_limit, idle, tls_failure, handshake_timeout, peer_io, admission_quota, frame_quota, partial_frame, login_timeout, output_timeout, negotiation_limit, identity_limit, authentication_limit, session_id_limit, session_limit, tcp_protection, count };
    std::array<std::optional<Clock::time_point>, static_cast<std::size_t>(Warning::count)> last_warnings{};
    auto warn_once = [&](Warning category, std::string_view message) {
        const auto now = Clock::now();
        auto& last = last_warnings[static_cast<std::size_t>(category)];
        if (!last || now - *last >= std::chrono::seconds(5)) {
            last = now;
            logger_.warning(message);
        }
    };

    auto pause_accepting = [&] {
        for (const auto& listener : listeners) {
            if (::epoll_ctl(epoll.get(), EPOLL_CTL_DEL, listener.get(), nullptr) == -1) {
                system_failure("pause TCP listener");
            }
        }
        accepting = false;
        resume_accepting = Clock::now() + std::chrono::seconds(1);
    };
    auto disconnect = [&](auto iterator, Event reason) {
        logger_.connection(reason, iterator->first, static_cast<bool>(iterator->second.tls));
        if (iterator->second.tls && !iterator->second.tls_ready) {
            adcs_incomplete_.fetch_add(1);
        }
        if (iterator->second.decoder.finish()) {
            adc_invalid_.fetch_add(1);
            warn_once(Warning::invalid_frame, "ADC invalid frame rejected; payload omitted");
        }
        // DEL before close; event tokens are never reused even when the OS reuses an fd.
        ::epoll_ctl(epoll.get(), EPOLL_CTL_DEL, iterator->second.fd.get(), nullptr);
        peer_limiter.release(iterator->second.peer_key, !login.session_model(iterator->first).was_ready());
        login.disconnect(iterator->first, reason != Event::shutdown, session_cause(reason), Clock::now());
        connections.erase(iterator);
        active_.fetch_sub(1);
        closed_.fetch_add(1);
    };
    auto count_security_close = [&](Connection& connection) {
        if (!connection.security_counted) { connection.security_counted=true; security_closed_.fetch_add(1); }
    };
    auto begin_close = [&](auto iterator, adc::CloseReason reason, Clock::time_point now) {
        login.close(iterator->first, reason, now);
    };
    auto expire = [&](auto iterator, Clock::time_point now) {
        auto& connection = iterator->second;
        if (login.closing(iterator->first)) {
            if (now - *login.session_model(iterator->first).timeline().closing >= options_.security.close_timeout) {
                disconnect(iterator, Event::close_timeout);
                return true;
            }
            return false; // Absolute terminal drain replaces input/login/idle timers.
        }
        const bool handshake = connection.tls && !connection.tls_ready;
        const bool handshake_expired = handshake &&
            now - login.session_model(iterator->first).timeline().accepted >= options_.security.tls_handshake_timeout;
        const bool partial_expired = connection.partial_started &&
            now - *connection.partial_started >= options_.security.partial_frame_timeout;
        const bool idle_expired = now - connection.last_activity >= options_.idle_timeout;
        const bool login_expired = !login.session_model(iterator->first).was_ready() &&
            now - login.session_model(iterator->first).timeline().accepted >= options_.security.login_timeout;
        const bool output_expired = connection.output_progress &&
            now - *connection.output_progress >= options_.security.output_timeout;
        if (!handshake_expired && !partial_expired && !idle_expired && !login_expired && !output_expired) { return false; }
        if (handshake_expired || (handshake && (idle_expired || login_expired))) {
            count_security_close(connection);
            warn_once(Warning::handshake_timeout, "ADCS handshake timed out; connection closed");
            disconnect(iterator, Event::handshake_timeout);
        } else if (partial_expired) {
            count_security_close(connection);
            warn_once(Warning::partial_frame, "Security partial frame timed out; connection closed");
            begin_close(iterator, adc::CloseReason::partial_frame_timeout, now);
            return false;
        } else if (login_expired || output_expired) {
            count_security_close(connection);
            warn_once(login_expired ? Warning::login_timeout : Warning::output_timeout,
                      login_expired ? "ADC login timed out; connection closed" : "ADC output stalled; connection closed");
            if (output_expired) { disconnect(iterator, Event::output_timeout); }
            else {
                begin_close(iterator, adc::CloseReason::login_timeout, now);
                return false;
            }
        } else {
            warn_once(Warning::idle, "Network idle connection closed");
            begin_close(iterator, adc::CloseReason::idle_timeout, now);
            return false;
        }
        return true;
    };
    while (!stop_.load()) {
        if (!accepting && Clock::now() >= resume_accepting) {
            for (std::size_t i = 0; i < listeners.size(); ++i) {
                watch(epoll.get(), listeners[i].get(), i + 1, EPOLLIN);
            }
            accepting = true;
        }
        // A relay can queue output for a peer other than the current event.
        // Refresh every interest before waiting; never poll EPOLLOUT without work.
        for (auto it = connections.begin(); it != connections.end();) {
            auto current = it++;
            const auto id = current->first;
            auto& connection = current->second;
            if (login.must_close(id)) {
                if (login.overloaded(id)) { count_security_close(connection); }
                disconnect(current, session_event(login.session_model(id).terminal_cause()));
                continue;
            }
            const bool pending = !login.output(id).empty();
            if (pending && !connection.output_progress) { connection.output_progress = Clock::now(); }
            if (!pending) { connection.output_progress.reset(); }
            bool read = !connection.want_write && !login.closing(id);
            bool write = connection.want_write || (connection.tls_ready && pending);
            if (connection.write_wait) {
                read = *connection.write_wait == adcs::IoStatus::want_read;
                write = *connection.write_wait == adcs::IoStatus::want_write;
            }
            const auto interest = (read ? EPOLLIN : 0U) | (write ? EPOLLOUT : 0U) | EPOLLRDHUP;
            if (interest != connection.interest) {
                update_client_interest(epoll.get(), connection.fd.get(), id, read, write);
                connection.interest = interest;
            }
        }
        // SSL_pending bytes have no corresponding kernel readiness event. Schedule
        // them explicitly while retaining the bounded per-client read budget.
        std::vector<epoll_event> buffered;
        for (const auto& [client_token, connection] : connections) {
            if (connection.tls && connection.tls_ready && !connection.want_write && !connection.write_wait &&
                connection.tls->pending()) {
                epoll_event ready{};
                ready.events = EPOLLIN;
                ready.data.u64 = client_token;
                buffered.push_back(ready);
            }
        }
        publish_users();
        const int count = ::epoll_wait(epoll.get(), events.data(), static_cast<int>(events.size()),
                                       buffered.empty() ? 250 : 0);
        if (count == -1) {
            if (errno == EINTR) { continue; }
            system_failure("epoll_wait");
        }
        std::vector<epoll_event> ready(events.begin(), events.begin() + count);
        for (const auto& pending : buffered) {
            if (std::none_of(ready.begin(), ready.begin() + count, [&](const auto& event) {
                    return event.data.u64 == pending.data.u64;
                })) {
                ready.push_back(pending);
            }
        }
        for (const auto& event : ready) {
            if (stop_.load()) { break; }
            const auto token = event.data.u64;
            if (token == 0) { continue; } // stop_ is authoritative; eventfd is not drained.
            if (token <= listeners.size()) {
                if (!accepting) { continue; }
                if ((event.events & (EPOLLERR | EPOLLHUP)) != 0) {
                    throw std::runtime_error("TCP listener failed");
                }
                // Bounded batches keep a connection flood from starving shutdown/read events.
                for (int batch = 0; batch < 64 && !stop_.load(); ++batch) {
                    sockaddr_storage remote{};
                    socklen_t remote_size = sizeof(remote);
                    Fd client(::accept4(listeners[static_cast<std::size_t>(token - 1)].get(),
                                        reinterpret_cast<sockaddr*>(&remote), &remote_size,
                                        SOCK_NONBLOCK | SOCK_CLOEXEC));
                    if (client.get() == -1) {
                        const int error = errno;
                        if (error == EAGAIN || error == EWOULDBLOCK) { break; }
                        if (error == EINTR || error == ECONNABORTED || error == EPROTO ||
                            error == ENETDOWN || error == ENOPROTOOPT || error == EHOSTDOWN ||
                            error == ENONET || error == EHOSTUNREACH || error == EOPNOTSUPP ||
                            error == ENETUNREACH) { continue; }
                        if (error == EMFILE || error == ENFILE || error == ENOBUFS || error == ENOMEM) {
                            logger_.error("TCP accept resource limit; retrying in one second");
                            pause_accepting();
                            break;
                        }
                        throw std::system_error(error, std::generic_category(), "accept4");
                    }
                    if (connections.size() >= options_.max_connections) {
                        rejected_.fetch_add(1);
                        warn_once(Warning::connection_limit, "Network connection limit reached; excess peer rejected");
                        continue; // RAII closes this excess client, without per-client log flooding.
                    }
                    if (login.sessions().size() >= login.sessions().capacity()) {
                        rejected_.fetch_add(1);
                        warn_once(Warning::session_limit,"User session limit reached; excess peer rejected");
                        continue; // No TLS, quota, model, SID or transport token allocated.
                    }
                    const auto admitted_at = Clock::now();
                    const auto key = peer_identity(remote);
                    if(options_.bans&&options_.bans->ip(std::string_view(key).substr(2),bans::Directory::now())){
                        rejected_.fetch_add(1);security_rejected_.fetch_add(1);
                        continue; // Drop banned addresses before TLS/model/SID allocation; no payload logs.
                    }
                    if (!peer_limiter.admit(key, admitted_at)) {
                        rejected_.fetch_add(1);
                        security_rejected_.fetch_add(1);
                        warn_once(Warning::admission_quota, "Security admission or pending-login quota reached; peer rejected");
                        continue;
                    }
                    const auto allocated = session_ids.acquire();
                    if (!allocated) {
                        peer_limiter.release(key);rejected_.fetch_add(1);
                        warn_once(Warning::session_id_limit,"Session identifier space exhausted; peer rejected");
                        continue;
                    }
                    const auto client_token = allocated->value();
                    if (!protect_tcp_socket(client.get(), options_.idle_timeout)) {
                        peer_limiter.release(key);
                        rejected_.fetch_add(1);
                        warn_once(Warning::tcp_protection, "TCP socket protection failed; peer rejected");
                        continue;
                    }
                    const bool secure = secure_listeners[static_cast<std::size_t>(token - 1)];
                    std::unique_ptr<adcs::Connection> tls;
                    if (secure) {
                        try {
                            tls = std::make_unique<adcs::Connection>(*tls_context, client.get());
                        } catch (const std::exception&) {
                            peer_limiter.release(key);
                            rejected_.fetch_add(1);
                            warn_once(Warning::tls_failure, "ADCS session initialization failed; peer rejected");
                            continue; // A failed peer session must not stop the listener.
                        }
                    }
                    try {
                        watch(epoll.get(), client.get(), client_token, EPOLLIN | EPOLLRDHUP);
                    } catch (const std::system_error& error) {
                        peer_limiter.release(key);
                        if (error.code().value() != ENOMEM && error.code().value() != ENOSPC) { throw; }
                        rejected_.fetch_add(1);
                        logger_.error("TCP epoll watch resource limit; retrying in one second");
                        pause_accepting();
                        break;
                    }
                    connections.emplace(client_token, Connection{
                        std::move(client), admitted_at, {}, std::move(tls), !secure, false,
                        key, std::nullopt, 0,
                        security::FrameLimiter(options_.security.max_frames_per_second), false, {}, {}, {}, EPOLLIN | EPOLLRDHUP, false,
                        security::NegotiationBudget(options_.security.max_login_bytes,options_.security.max_login_frames),false,false});
                    login.connect(client_token, std::string_view(key).substr(2),
                                  secure ? users::Transport::adcs : users::Transport::adc, admitted_at);
                    const auto active = active_.fetch_add(1) + 1;
                    peak_connections_.store(std::max(peak_connections_.load(), active));
                    accepted_.fetch_add(1);
                    logger_.connection(Event::accepted, client_token, secure);
                }
                continue;
            }
            const auto found = connections.find(token);
            if (found == connections.end()) { continue; } // stale event from a closed connection
            // Check absolute deadlines before allowing newly readable bytes to
            // complete a frame or restart an idle timer.
            if (expire(found, Clock::now())) { continue; }
            const adc::StreamDecoder::Sink record_frame = [&](adc::ParseResult result) {
                auto& connection = found->second;
                if (!connection.frame_budget.admit(Clock::now())) {
                    connection.security_violation = true;
                    return;
                }
                if (!login.session_model(token).was_ready() && !login.closing(token) && !connection.negotiation.admit_frame()) {
                    connection.security_violation = connection.negotiation_violation = true;
                    return;
                }
                if (const auto* message = std::get_if<adc::Message>(&result)) {
                    adc_frames_.fetch_add(1);
                    if (message->kind == adc::Command::unknown) { adc_unknown_.fetch_add(1); }
                    if (login.needs_identity_check(token,*message) && !peer_limiter.identity_check(connection.peer_key,Clock::now())) {
                        connection.security_violation = connection.identity_violation = true;
                        return; // No INF validation/Tiger work on a denied attempt.
                    }
                    const auto before_chat = login.chat_statistics();
                    const bool was_closing = login.closing(token);
                    login.receive(token, *message);
                    if (!was_closing && login.session_model(token).terminal_cause() == Cause::authentication_limit) {
                        security_closed_.fetch_add(1);
                        warn_once(Warning::authentication_limit,"Authentication attempt limit reached; connection closed");
                    }
                    const auto after_chat = login.chat_statistics();
                    for (std::size_t i = 0; i < chat::event_names.size(); ++i) {
                        if (before_chat.values[i+1] != after_chat.values[i+1]) {
                            logger_.chat_event(static_cast<chat::Event>(i));
                        }
                    }
                } else if (std::holds_alternative<adc::KeepAlive>(result)) {
                    adc_keepalives_.fetch_add(1);
                } else {
                    adc_invalid_.fetch_add(1);
                    ++connection.invalid_frames;
                    warn_once(Warning::invalid_frame, "ADC invalid frame rejected; payload omitted");
                    if (connection.invalid_frames >= options_.security.max_invalid_frames) {
                        connection.security_violation = true;
                    }
                }
            };
            bool removed = false;
            bool closed = false;
            auto close_reason = Event::peer_closed;
            auto change_interest = [&](bool write) {
                found->second.want_write = write;
            };
            if (found->second.tls && !found->second.tls_ready) {
                const auto result = found->second.tls->handshake();
                if (expire(found, Clock::now())) { continue; }
                if (result.status == adcs::IoStatus::complete) {
                    found->second.tls_ready = true;
                    login.transport_ready(token,Clock::now());
                    found->second.last_activity = Clock::now();
                    adcs_handshakes_.fetch_add(1);
                    logger_.connection(Event::tls_ready, token, true);
                    change_interest(false);
                } else if (result.status == adcs::IoStatus::want_write) {
                    change_interest(true);
                } else if (result.status == adcs::IoStatus::want_read) {
                    change_interest(false);
                } else {
                    closed = true;
                    close_reason = Event::tls_handshake_failed;
                    warn_once(Warning::tls_failure, "ADCS handshake failed; connection closed");
                }
            }
            // Retry a pinned SSL write before any SSL_read. Queue storage and
            // the byte range stay unchanged across WANT_READ/WANT_WRITE.
            auto flush_output = [&] {
                auto& connection = found->second;
                if (!connection.tls_ready || connection.want_write) { return; }
                if (connection.write_wait &&
                    (event.events & (*connection.write_wait == adcs::IoStatus::want_read ? EPOLLIN : EPOLLOUT)) == 0) { return; }
                for (int batch = 0; batch < 8 && !stop_.load(); ++batch) {
                    const auto output = login.output(token);
                    if (output.empty() || login.must_close(token)) { break; }
                    std::size_t sent = 0;
                    if (connection.tls) {
                        const auto result = connection.tls->send(output.data(), output.size());
                        if (result.status == adcs::IoStatus::want_read || result.status == adcs::IoStatus::want_write) {
                            connection.write_wait = result.status;
                            break;
                        }
                        connection.write_wait.reset();
                        if (result.status != adcs::IoStatus::complete) { closed = true; close_reason = Event::write_error; break; }
                        sent = result.bytes;
                    } else {
                        const auto result = ::send(connection.fd.get(), output.data(), output.size(), MSG_NOSIGNAL);
                        if (result > 0) { sent = static_cast<std::size_t>(result); }
                        else if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { break; }
                        else if (result < 0 && errno == EINTR) { continue; }
                        else { closed = true; close_reason = Event::write_error; break; }
                    }
                    if (sent == 0) { closed = true; close_reason = Event::write_error; break; }
                    sent_bytes_.fetch_add(static_cast<std::uint64_t>(sent));
                    const bool became_ready = login.consume(token, sent);
                    connection.output_progress = Clock::now();
                    if (became_ready) {
                        peer_limiter.complete_login(connection.peer_key);
                        logger_.connection(Event::login_ready, token, static_cast<bool>(connection.tls));
                    }
                }
                if (login.output(token).empty()) { connection.output_progress.reset(); }
            };
            if (!closed && found->second.tls_ready && !found->second.want_write) { flush_output(); }
            const bool readable = found->second.want_write ? (event.events & EPOLLOUT) != 0 :
                (event.events & EPOLLIN) != 0 || (found->second.tls && found->second.tls->pending());
            if (!closed && found->second.tls_ready && !found->second.write_wait &&
                (!login.closing(token) || found->second.want_write) && readable) {
                for (int batch = 0; batch < 8 && !stop_.load(); ++batch) {
                    if (expire(found, Clock::now())) { removed = true; break; }
                    std::size_t size = 0;
                    if (found->second.tls) {
                        const auto result = found->second.tls->receive(found->second.input.data(), found->second.input.size());
                        if (result.status == adcs::IoStatus::complete) {
                            size = result.bytes;
                            change_interest(false);
                        } else if (result.status == adcs::IoStatus::want_read ||
                                   result.status == adcs::IoStatus::want_write) {
                            change_interest(result.status == adcs::IoStatus::want_write);
                            break;
                        } else {
                            closed = true;
                            close_reason = result.status == adcs::IoStatus::closed
                                ? Event::tls_closed : Event::tls_read_error;
                            if (result.status != adcs::IoStatus::closed) {
                                warn_once(Warning::peer_io, "Network peer I/O failed; connection closed");
                            }
                            break;
                        }
                    } else {
                        const auto received = ::recv(found->second.fd.get(), found->second.input.data(), found->second.input.size(), 0);
                        if (received > 0) {
                            size = static_cast<std::size_t>(received);
                        } else if (received == 0) {
                            closed = true;
                            break;
                        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        } else if (errno != EINTR) {
                            closed = true;
                            close_reason = Event::tcp_read_error;
                            warn_once(Warning::peer_io, "Network peer I/O failed; connection closed");
                            break;
                        } else {
                            continue;
                        }
                    }
                    if (size == 0) { break; }
                    if (login.closing(token)) {
                        // Finish an existing SSL_read WANT_WRITE dependency,
                        // then discard plaintext and resume terminal output.
                        received_bytes_.fetch_add(static_cast<std::uint64_t>(size));
                        break;
                    }
                    if (expire(found, Clock::now())) { removed = true; break; }
                    received_bytes_.fetch_add(static_cast<std::uint64_t>(size));
                    found->second.last_activity = Clock::now();
                    // At most one decoder result per segment. Stop immediately
                    // after a quota breach rather than parsing the remainder.
                    auto input = std::string_view(found->second.input.data(), size);
                    while (!input.empty() && !found->second.security_violation) {
                        if (expire(found, Clock::now())) { removed = true; break; }
                        const auto lf = input.find('\n');
                        const auto length = lf == std::string_view::npos ? input.size() : lf + 1;
                        auto& connection = found->second;
                        if (!login.session_model(token).was_ready() && !login.closing(token) && !connection.negotiation.admit_bytes(length)) {
                            connection.security_violation = connection.negotiation_violation = true;
                            break; // Includes unfinished/discarded bytes, before decoder allocation/work.
                        }
                        if (!connection.partial_started) { connection.partial_started = Clock::now(); }
                        connection.decoder.feed(input.substr(0, length), record_frame);
                        if (!connection.decoder.in_progress()) { connection.partial_started.reset(); }
                        input.remove_prefix(length);
                    }
                    if (removed) { break; }
                    if (found->second.security_violation) {
                        count_security_close(found->second);
                        if (found->second.negotiation_violation) {
                            warn_once(Warning::negotiation_limit, "Security negotiation limit reached; connection closed");
                            begin_close(found, adc::CloseReason::negotiation_limit, Clock::now());
                        } else if (found->second.identity_violation) {
                            warn_once(Warning::identity_limit, "Security identity verification limit reached; connection closed");
                            begin_close(found, adc::CloseReason::identity_limit, Clock::now());
                        } else {
                            warn_once(Warning::frame_quota, "Security frame quota reached; connection closed");
                            begin_close(found, adc::CloseReason::security_quota, Clock::now());
                        }
                        break;
                    }
                }
            }
            if (removed) { continue; }
            if (!closed && !found->second.write_wait) { flush_output(); }
            if (login.must_close(token)) {
                if (login.overloaded(token)) { count_security_close(found->second); }
                closed = true;
                close_reason = session_event(login.session_model(token).terminal_cause());
            }
            if (closed || (event.events & (EPOLLERR | EPOLLHUP)) != 0) {
                if (!closed) { warn_once(Warning::peer_io, "Network peer I/O failed; connection closed"); }
                disconnect(found, closed ? close_reason : Event::socket_error);
            }
        }
        const auto now = Clock::now();
        for (auto it = connections.begin(); it != connections.end();) {
            auto current = it++;
            (void)expire(current, now);
        }
    }
    // Close listeners first, then active clients, before reporting stopped.
    listeners.clear();
    while (!connections.empty()) { disconnect(connections.begin(), Event::shutdown); }
    active_.store(0);
    publish_users();
    logger_.info("Users stopped; online=" + std::to_string(online_users_.load()) +
                 " peak_online=" + std::to_string(peak_online_users_.load()) +
                 " successful_logins=" + std::to_string(successful_logins_.load()));
    logger_.info("Network stopped; all TCP sockets closed; accepted=" + std::to_string(accepted_.load()) +
                 " rejected=" + std::to_string(rejected_.load()) +
                 " received_bytes=" + std::to_string(received_bytes_.load()) +
                 " sent_bytes=" + std::to_string(sent_bytes_.load()) +
                 " closed=" + std::to_string(closed_.load()) +
                 " peak_connections=" + std::to_string(peak_connections_.load()));
    logger_.info("Security stopped; rejected=" + std::to_string(security_rejected_.load()) +
                 " closed=" + std::to_string(security_closed_.load()));
    logger_.info("ADC stopped; frames=" + std::to_string(adc_frames_.load()) +
                 " unknown_commands=" + std::to_string(adc_unknown_.load()) +
                 " invalid_frames=" + std::to_string(adc_invalid_.load()) +
                 " keepalives=" + std::to_string(adc_keepalives_.load()));
    if (tls_context) {
        logger_.info("ADCS stopped; handshakes=" + std::to_string(adcs_handshakes_.load()) +
                     " incomplete=" + std::to_string(adcs_incomplete_.load()));
    }
}

} // namespace dc24h::network
