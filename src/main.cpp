#include "dc24h/core/server.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/version.hpp"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "dc24h.eu " << dc24h::version << '\n';
        return EXIT_SUCCESS;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: dc24h [--help | --version]\n"
                     "Run the dc24h.eu Core in the foreground.\n"
                     "Stop gracefully with SIGINT (Ctrl+C) or SIGTERM.\n";
        return EXIT_SUCCESS;
    }
    if (argc != 1) {
        std::cerr << "Invalid arguments. Use dc24h --help.\n";
        return 2;
    }

    dc24h::logging::Logger logger;
    try {
        const dc24h::core::TerminationSignals signals;
        dc24h::core::Server server(logger);
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
