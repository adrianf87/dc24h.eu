#include "dc24h/network/tcp_server.hpp"
#include "dc24h/core/server.hpp"
#include "dc24h/logging/logger.hpp"

#include <arpa/inet.h>
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
        lifecycle_and_rollback(logger);
        std::cout << "PASS: endpoints, limits, early stop, single use, Core failure state, descriptor rollback\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
