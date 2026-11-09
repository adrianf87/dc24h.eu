#include "dc24h/network/tcp_server.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/adc/parser.hpp"

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace dc24h::network {
namespace {
using Clock = std::chrono::steady_clock;

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

struct Connection {
    Fd fd;
    Clock::time_point last_activity;
    adc::StreamDecoder decoder;
};
} // namespace

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
    if (options_.listeners.empty() || options_.listeners.size() > 16 ||
        options_.max_connections == 0 || options_.max_connections > 65535 ||
        options_.idle_timeout < std::chrono::seconds(1) ||
        options_.idle_timeout > std::chrono::hours(24)) {
        throw std::invalid_argument("Invalid TCP limits (listeners 1..16, connections 1..65535, idle 1..86400s)");
    }
    for (const auto& endpoint : options_.listeners) {
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
    return {active_.load(), accepted_.load(), rejected_.load(), received_bytes_.load(),
            adc_frames_.load(), adc_unknown_.load(), adc_invalid_.load(), adc_keepalives_.load()};
}

void TcpServer::run() {
    if (used_.exchange(true)) { throw std::logic_error("TcpServer::run() may only be called once"); }
    if (stop_.load()) { return; }

    // Reset active on all exits, including exceptions after partially completed startup.
    struct ResetActive {
        std::atomic<std::size_t>& value;
        ~ResetActive() { value.store(0); }
    } reset{active_};
    Fd epoll(::epoll_create1(EPOLL_CLOEXEC));
    if (epoll.get() == -1) { system_failure("epoll_create1"); }
    watch(epoll.get(), wake_fd_, 0, EPOLLIN);
    std::vector<Fd> listeners;
    std::vector<std::string> addresses;
    for (const auto& endpoint : options_.listeners) {
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
        if (::listen(listener.get(), SOMAXCONN) == -1) { system_failure("listen"); }
        watch(epoll.get(), listener.get(), listeners.size() + 1, EPOLLIN);
        addresses.push_back(bound_address(listener.get()));
        listeners.push_back(std::move(listener));
    }
    // Log readiness only after every configured listener has started successfully.
    for (const auto& address : addresses) { logger_.info("Network listening on " + address); }
    std::unordered_map<std::uint64_t, Connection> connections;
    std::uint64_t next_token = listeners.size() + 1;
    std::array<epoll_event, 64> events{};
    std::array<char, 8192> buffer{};
    bool accepting = true;
    Clock::time_point resume_accepting{};

    auto pause_accepting = [&] {
        for (const auto& listener : listeners) {
            if (::epoll_ctl(epoll.get(), EPOLL_CTL_DEL, listener.get(), nullptr) == -1) {
                system_failure("pause TCP listener");
            }
        }
        accepting = false;
        resume_accepting = Clock::now() + std::chrono::seconds(1);
    };
    const adc::StreamDecoder::Sink record_frame = [&](adc::ParseResult result) {
        if (const auto* message = std::get_if<adc::Message>(&result)) {
            adc_frames_.fetch_add(1);
            if (message->kind == adc::Command::unknown) { adc_unknown_.fetch_add(1); }
        } else if (std::holds_alternative<adc::KeepAlive>(result)) {
            adc_keepalives_.fetch_add(1);
        } else {
            adc_invalid_.fetch_add(1);
        }
        // Parsing milestone only: no dispatch, session state or responses yet.
        // Never log untrusted parameters, private identifiers or password hashes.
    };
    auto disconnect = [&](auto iterator) {
        if (iterator->second.decoder.finish()) { adc_invalid_.fetch_add(1); }
        // DEL before close; event tokens are never reused even when the OS reuses an fd.
        ::epoll_ctl(epoll.get(), EPOLL_CTL_DEL, iterator->second.fd.get(), nullptr);
        connections.erase(iterator);
        active_.fetch_sub(1);
    };
    while (!stop_.load()) {
        if (!accepting && Clock::now() >= resume_accepting) {
            for (std::size_t i = 0; i < listeners.size(); ++i) {
                watch(epoll.get(), listeners[i].get(), i + 1, EPOLLIN);
            }
            accepting = true;
        }
        const int count = ::epoll_wait(epoll.get(), events.data(), static_cast<int>(events.size()), 250);
        if (count == -1) {
            if (errno == EINTR) { continue; }
            system_failure("epoll_wait");
        }
        for (int i = 0; i < count && !stop_.load(); ++i) {
            const auto& event = events[static_cast<std::size_t>(i)];
            const auto token = event.data.u64;
            if (token == 0) { continue; } // stop_ is authoritative; eventfd is not drained.
            if (token <= listeners.size()) {
                if (!accepting) { continue; }
                if ((event.events & (EPOLLERR | EPOLLHUP)) != 0) {
                    throw std::runtime_error("TCP listener failed");
                }
                // Bounded batches keep a connection flood from starving shutdown/read events.
                for (int batch = 0; batch < 64 && !stop_.load(); ++batch) {
                    Fd client(::accept4(listeners[static_cast<std::size_t>(token - 1)].get(),
                                        nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC));
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
                        continue; // RAII closes this excess client, without per-client log flooding.
                    }
                    const auto client_token = next_token++;
                    try {
                        watch(epoll.get(), client.get(), client_token, EPOLLIN | EPOLLRDHUP);
                    } catch (const std::system_error& error) {
                        if (error.code().value() != ENOMEM && error.code().value() != ENOSPC) { throw; }
                        rejected_.fetch_add(1);
                        logger_.error("TCP epoll watch resource limit; retrying in one second");
                        pause_accepting();
                        break;
                    }
                    connections.emplace(client_token, Connection{std::move(client), Clock::now(), {}});
                    active_.fetch_add(1);
                    accepted_.fetch_add(1);
                }
                continue;
            }
            const auto found = connections.find(token);
            if (found == connections.end()) { continue; } // stale event from a closed connection
            bool closed = false;
            for (int batch = 0; batch < 8 && !stop_.load(); ++batch) {
                const auto received = ::recv(found->second.fd.get(), buffer.data(), buffer.size(), 0);
                if (received > 0) {
                    received_bytes_.fetch_add(static_cast<std::uint64_t>(received));
                    found->second.last_activity = Clock::now();
                    found->second.decoder.feed(
                        std::string_view(buffer.data(), static_cast<std::size_t>(received)), record_frame);
                } else if (received == 0) {
                    closed = true;
                    break;
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else if (errno != EINTR) {
                    closed = true; // isolated peer failure, including reset
                    break;
                }
            }
            if (closed || (event.events & (EPOLLERR | EPOLLHUP)) != 0) { disconnect(found); }
        }
        const auto now = Clock::now();
        for (auto it = connections.begin(); it != connections.end();) {
            if (now - it->second.last_activity >= options_.idle_timeout) {
                auto expired = it++;
                disconnect(expired);
            } else { ++it; }
        }
    }
    // Close listeners first, then active clients, before reporting stopped.
    listeners.clear();
    while (!connections.empty()) { disconnect(connections.begin()); }
    active_.store(0);
    logger_.info("Network stopped; all TCP sockets closed; accepted=" + std::to_string(accepted_.load()) +
                 " rejected=" + std::to_string(rejected_.load()) +
                 " received_bytes=" + std::to_string(received_bytes_.load()));
    logger_.info("ADC stopped; frames=" + std::to_string(adc_frames_.load()) +
                 " unknown_commands=" + std::to_string(adc_unknown_.load()) +
                 " invalid_frames=" + std::to_string(adc_invalid_.load()) +
                 " keepalives=" + std::to_string(adc_keepalives_.load()));
}

} // namespace dc24h::network
