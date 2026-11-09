#include "dc24h/adcs/tls.hpp"
#include "dc24h/configuration/configuration.hpp"
#include "dc24h/core/server.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/network/tcp_server.hpp"
#include "dc24h/version.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

int main(int argc, char* argv[]) {
    // A TLS write after a peer closes must fail locally, not kill the process.
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        std::cerr << "Cannot configure SIGPIPE handling\n";
        return EXIT_FAILURE;
    }
    // Select CLI error language without treating another option's value as a key.
    auto language = dc24h::i18n::Language::us;
    for (int i = 1; i < argc; ++i) {
        const std::string_view key(argv[i]);
        if (key == "--help" || key == "--version" || key == "--check-config") { continue; }
        if (i + 1 < argc) {
            const std::string_view value(argv[++i]);
            if (key == "--language" && (value == "us" || value == "pl")) {
                language = dc24h::i18n::parse_language(value);
            }
        }
    }
    dc24h::configuration::CommandLine parsed;
    try {
        parsed = dc24h::configuration::parse_arguments(argc, argv);
        language = parsed.settings.logging.language;
    } catch (const std::exception& error) {
        std::cerr << dc24h::i18n::translate("Invalid arguments: ", language)
                  << dc24h::i18n::translate(error.what(), language)
                  << dc24h::i18n::translate(". Use dc24h --help.\n", language);
        return 2;
    }

    if (parsed.show_help) {
        std::cout << dc24h::i18n::help(language);
        return EXIT_SUCCESS;
    }
    if (parsed.show_version) {
        std::cout << "dc24h.eu " << dc24h::version << '\n';
        return EXIT_SUCCESS;
    }
    if (parsed.check_only) {
        try {
            if (!parsed.settings.network.tls_listeners.empty()) {
                const dc24h::adcs::Context context(parsed.settings.network.tls_certificate,
                                                   parsed.settings.network.tls_private_key);
            }
            std::cout << dc24h::i18n::translate("Configuration OK (network ports not bound; log file not opened)\n", language);
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << dc24h::i18n::translate("Configuration check failed: ", language)
                      << dc24h::i18n::translate(error.what(), language) << '\n';
            return 2;
        }
    }

    try {
        dc24h::logging::Logger logger(parsed.settings.logging);
        try {
            const dc24h::core::TerminationSignals signals;
            logger.info(std::string("Starting dc24h.eu ") + std::string(dc24h::version));
            logger.info("Startup settings: adc_listeners=" + std::to_string(parsed.settings.network.listeners.size()) +
                    " adcs_listeners=" + std::to_string(parsed.settings.network.tls_listeners.size()) +
                    " max_connections=" + std::to_string(parsed.settings.network.max_connections) +
                    " idle_timeout_seconds=" + std::to_string(parsed.settings.network.idle_timeout.count()) +
                    " connection_logging=" + (parsed.settings.logging.connections ? "on" : "off"));
            dc24h::network::TcpServer network(logger, std::move(parsed.settings.network));
            dc24h::core::Server server(logger, &network);
            std::atomic<bool> signal_error{false};

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
    } catch (const std::exception& error) {
        std::cerr << dc24h::i18n::translate("Startup failure: ", language)
                  << dc24h::i18n::translate(error.what(), language) << '\n';
        return EXIT_FAILURE;
    }
}
