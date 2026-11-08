#include "dc24h/core/server.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/network/tcp_server.hpp"
#include "dc24h/version.hpp"

#include <atomic>
#include <charconv>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {
unsigned int number(std::string_view text, unsigned int maximum) {
    unsigned int value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value == 0 || value > maximum) {
        throw std::invalid_argument("Numeric option outside allowed range");
    }
    return value;
}

dc24h::network::Options parse_options(int argc, char* argv[]) {
    dc24h::network::Options options;
    bool custom_listeners = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view key(argv[i]);
        if (key != "--listen" && key != "--max-connections" && key != "--idle-timeout") {
            throw std::invalid_argument("Unknown option");
        }
        if (++i == argc) { throw std::invalid_argument("Missing option value"); }
        const std::string_view value(argv[i]);
        if (key == "--listen") {
            if (!custom_listeners) { options.listeners.clear(); custom_listeners = true; }
            if (options.listeners.size() == 16) { throw std::invalid_argument("At most 16 listeners"); }
            options.listeners.push_back(dc24h::network::parse_endpoint(value));
        } else if (key == "--max-connections") {
            options.max_connections = number(value, 65535);
        } else {
            options.idle_timeout = std::chrono::seconds(number(value, 86400));
        }
    }
    return options;
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "dc24h.eu " << dc24h::version << '\n';
        return EXIT_SUCCESS;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: dc24h [--listen IPv4:port | [IPv6]:port] ...\n"
                     "             [--max-connections N] [--idle-timeout SECONDS]\n"
                     "       dc24h --help | --version\n"
                     "Run dc24h.eu in the foreground. Stop with SIGINT (Ctrl+C) or SIGTERM.\n"
                     "Defaults: --listen 127.0.0.1:1511 --max-connections 1024 --idle-timeout 120\n"
                     "Repeat --listen for up to 16 ports; port 0 selects an ephemeral port.\n"
                     "Limits: connections 1..65535; idle timeout 1..86400 seconds.\n"
                     "TCP transport only: incoming bytes are discarded; ADC/ADCS is not implemented.\n";
        return EXIT_SUCCESS;
    }
    dc24h::network::Options options;
    try {
        options = parse_options(argc, argv);
    } catch (const std::invalid_argument& error) {
        // Never include untrusted option values in logs or terminal diagnostics.
        std::cerr << "Invalid arguments: " << error.what() << ". Use dc24h --help.\n";
        return 2;
    }

    dc24h::logging::Logger logger;
    try {
        const dc24h::core::TerminationSignals signals;
        dc24h::network::TcpServer network(logger, std::move(options));
        dc24h::core::Server server(logger, &network);
        std::atomic<bool> signal_error{false};
        logger.info(std::string("Starting dc24h.eu ") + std::string(dc24h::version));

        std::jthread signal_thread([&](std::stop_token stop) {
            try {
                if (const auto signal = signals.wait(stop)) {
                    logger.info(*signal == SIGINT ? "Received SIGINT; requesting stop"
                                                 : "Received SIGTERM; requesting stop");
                    server.request_stop();
                }
            } catch (const std::exception& error) {
                logger.error(error.what());
                signal_error.store(true);
                server.request_stop();
            }
        });

        server.run();
        signal_thread.request_stop();
        signal_thread.join();
        return signal_error.load() ? EXIT_FAILURE : EXIT_SUCCESS;
    } catch (const std::exception& error) {
        logger.error(error.what());
        return EXIT_FAILURE;
    }
}
