#include "dc24h/core/server.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/network/tcp_server.hpp"
#include "dc24h/version.hpp"

#include <atomic>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

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
        if (key != "--listen" && key != "--adcs-listen" && key != "--tls-cert" &&
            key != "--tls-key" && key != "--max-connections" && key != "--idle-timeout") {
            throw std::invalid_argument("Unknown option");
        }
        if (++i == argc) { throw std::invalid_argument("Missing option value"); }
        const std::string_view value(argv[i]);
        if (key == "--listen") {
            if (!custom_listeners) { options.listeners.clear(); custom_listeners = true; }
            if (options.listeners.size() == 16) { throw std::invalid_argument("At most 16 listeners"); }
            options.listeners.push_back(dc24h::network::parse_endpoint(value));
        } else if (key == "--adcs-listen") {
            if (!custom_listeners) { options.listeners.clear(); custom_listeners = true; }
            if (options.tls_listeners.size() == 16) { throw std::invalid_argument("At most 16 listeners"); }
            options.tls_listeners.push_back(dc24h::network::parse_endpoint(value));
        } else if (key == "--tls-cert") {
            options.tls_certificate = value;
        } else if (key == "--tls-key") {
            options.tls_private_key = value;
        } else if (key == "--max-connections") {
            options.max_connections = number(value, 65535);
        } else {
            options.idle_timeout = std::chrono::seconds(number(value, 86400));
        }
    }
    if (options.listeners.size() + options.tls_listeners.size() > 16 ||
        (options.tls_listeners.empty() && (!options.tls_certificate.empty() || !options.tls_private_key.empty())) ||
        (!options.tls_listeners.empty() &&
         (options.tls_certificate.empty() || options.tls_private_key.empty()))) {
        throw std::invalid_argument("ADCS listeners require both a TLS certificate and private key; maximum 16 listeners in total");
    }
    return options;
}
} // namespace

int main(int argc, char* argv[]) {
    // TLS handshakes can write to sockets after a peer aborts. A broken pipe
    // must become a per-connection I/O error, never terminate the hub process.
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        std::cerr << "Cannot configure SIGPIPE handling\n";
        return EXIT_FAILURE;
    }
    auto language = dc24h::i18n::Language::us;
    dc24h::network::Options options;
    try {
        // Consume only option keys: a value equal to --language stays a value.
        std::vector<char*> arguments{argv[0]};
        for (int i = 1; i < argc; ++i) {
            const std::string_view key(argv[i]);
            if (key == "--language") {
                if (++i == argc) { throw std::invalid_argument("Missing option value"); }
                language = dc24h::i18n::parse_language(argv[i]);
            } else {
                arguments.push_back(argv[i]);
                if (key != "--help" && key != "--version" && i + 1 < argc) {
                    arguments.push_back(argv[++i]);
                }
            }
        }
        if (arguments.size() == 2 && std::string_view(arguments[1]) == "--version") {
            std::cout << "dc24h.eu " << dc24h::version << '\n';
            return EXIT_SUCCESS;
        }
        if (arguments.size() == 2 && std::string_view(arguments[1]) == "--help") {
            std::cout << dc24h::i18n::help(language);
            return EXIT_SUCCESS;
        }
        options = parse_options(static_cast<int>(arguments.size()), arguments.data());
    } catch (const std::invalid_argument& error) {
        // Never include untrusted option values in logs or terminal diagnostics.
        std::cerr << dc24h::i18n::translate("Invalid arguments: ", language)
                  << dc24h::i18n::translate(error.what(), language)
                  << dc24h::i18n::translate(". Use dc24h --help.\n", language);
        return 2;
    }

    dc24h::logging::Logger logger(language);
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
