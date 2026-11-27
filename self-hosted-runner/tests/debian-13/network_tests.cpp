#include "dc24h/network/tcp_server.hpp"
#include "dc24h/core/server.hpp"
#include "dc24h/logging/logger.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

template<class Action> void invalid(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Invalid configuration was accepted");
}

std::size_t fd_count() {
    std::size_t count = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) { ++count; }
    return count;
}

void parser() {
    const auto v4 = dc24h::network::parse_endpoint("127.0.0.1:65535");
    require(v4.address == "127.0.0.1" && v4.port == 65535, "IPv4 parse failed");
    const auto v6 = dc24h::network::parse_endpoint("[::1]:0");
    require(v6.address == "::1" && v6.port == 0, "IPv6 parse failed");
    for (const auto* input : {"", "127.0.0.1", ":1", "localhost:1", "[::1]", "::1:1",
                              "[127.0.0.1]:1", "[::1]:65536", "127.0.0.1:-1",
                              "127.0.0.1:+1", "127.0.0.1:", "127.0.0.1:1x",
                              "127.0.0.1:999999999999999", "[::1]: 1", "[::1]:1:2"}) {
        invalid([&] { (void)dc24h::network::parse_endpoint(input); });
    }
}

void limits(dc24h::logging::Logger& logger) {
    using dc24h::network::Options;
    std::vector<Options> bad(12);
    bad[0].listeners.clear();
    bad[1].listeners.resize(17, {"127.0.0.1", 0});
    bad[2].max_connections = 0;
    bad[3].max_connections = 65536;
    bad[4].idle_timeout = std::chrono::seconds(0);
    bad[5].idle_timeout = std::chrono::seconds(86401);
    bad[6].listeners = {{"not-an-address", 0}};
    bad[7].listeners = {{std::string("127.0.0.1\0junk", 14), 0}};
    bad[8].users.max_sessions=0;
    bad[9].users.max_sessions=65536;
    bad[10].users.max_users=0;
    bad[11].users.max_users=65536;
    for (const auto& options : bad) {
        invalid([&] { dc24h::network::TcpServer network(logger, options); });
    }
}

int option_value(int fd, int level, int name) {
    int value = 0;
    socklen_t size = sizeof(value);
    require(::getsockopt(fd, level, name, &value, &size) == 0 && size == sizeof(value), "getsockopt failed");
    return value;
}

void tcp_protection() {
    require(dc24h::network::listen_backlog == 128, "Listen backlog drifted");
    require(dc24h::network::socket_buffer_bytes == 65536, "Socket buffer drifted");
    require(dc24h::network::read_buffer_bytes == 8192, "Read buffer drifted");
    require(!dc24h::network::protect_tcp_socket(-1, std::chrono::milliseconds(1000)), "Bad descriptor accepted");
    require(!dc24h::network::protect_tcp_socket(0, std::chrono::milliseconds(0)), "Zero timeout accepted");
    const int listener = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    require(listener >= 0, "Cannot create TCP listener");
    struct ListenerCleanup { int fd; ~ListenerCleanup() { ::close(fd); } } listener_cleanup{listener};
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "Cannot bind TCP fixture");
    require(::listen(listener, dc24h::network::listen_backlog) == 0, "Cannot listen on TCP fixture");
    socklen_t size = sizeof(address);
    require(::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0, "Cannot read fixture port");
    const int client = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    require(client >= 0, "Cannot create TCP client");
    struct ClientCleanup { int fd; ~ClientCleanup() { ::close(fd); } } client_cleanup{client};
    require(::connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "Cannot connect TCP fixture");
    const int accepted = ::accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
    require(accepted >= 0, "Cannot accept TCP fixture");
    struct AcceptedCleanup { int fd; ~AcceptedCleanup() { ::close(fd); } } accepted_cleanup{accepted};
    require(dc24h::network::protect_tcp_socket(accepted, std::chrono::milliseconds(1500)), "TCP protection failed");
    const int receive = option_value(accepted, SOL_SOCKET, SO_RCVBUF);
    const int send = option_value(accepted, SOL_SOCKET, SO_SNDBUF);
    require(receive >= dc24h::network::socket_buffer_bytes && receive <= dc24h::network::socket_buffer_bytes * 2,
            "Receive buffer outside the requested cap");
    require(send >= dc24h::network::socket_buffer_bytes && send <= dc24h::network::socket_buffer_bytes * 2,
            "Send buffer outside the requested cap");
    require(option_value(accepted, IPPROTO_TCP, TCP_USER_TIMEOUT) == 1500, "TCP user timeout was not applied");
}

void lifecycle_and_rollback(dc24h::logging::Logger& logger) {
    // Keep a real bound socket open so startup failure is deterministic, without a port race.
    const int occupied = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    require(occupied >= 0, "Cannot create occupied port fixture");
    struct Cleanup { int fd; ~Cleanup() { ::close(fd); } } cleanup{occupied};
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(::bind(occupied, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "Cannot bind fixture");
    require(::listen(occupied, 1) == 0, "Cannot listen on fixture");
    socklen_t size = sizeof(address);
    require(::getsockname(occupied, reinterpret_cast<sockaddr*>(&address), &size) == 0, "Cannot get fixture port");
    dc24h::network::Options options;
    options.listeners = {{"127.0.0.1", 0}, {"127.0.0.1", ntohs(address.sin_port)}};

    dc24h::network::TcpServer cancelled(logger, options);
    cancelled.request_stop();
    cancelled.request_stop();
    cancelled.run(); // Would fail to bind the second listener if early stop were lost.
    require(cancelled.statistics().active == 0, "Cancelled server has active clients");
    bool rejected = false;
    try { cancelled.run(); } catch (const std::logic_error&) { rejected = true; }
    require(rejected, "Reusing Network run was accepted");

    const auto baseline = fd_count();
    for (int iteration = 0; iteration < 30; ++iteration) {
        dc24h::network::TcpServer network(logger, options);
        dc24h::core::Server core(logger, &network);
        const auto before = fd_count();
        bool failed = false;
        try { core.run(); } catch (const std::system_error&) { failed = true; }
        require(failed, "Occupied listener startup unexpectedly succeeded");
        require(core.state() == dc24h::core::State::stopped, "Core state not stopped after startup failure");
        require(network.statistics().active == 0, "Failed Network has active clients");
        require(fd_count() == before, "Partial listener startup leaked descriptors");
    }
    require(fd_count() == baseline, "Network lifetime leaked eventfd descriptors");
}
} // namespace

int main() {
    try {
        dc24h::logging::Logger logger;
        parser();
        limits(logger);
        tcp_protection();
        lifecycle_and_rollback(logger);
        std::cout << "PASS: endpoints, limits, TCP protection, early stop, single use, Core failure state, descriptor rollback\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
