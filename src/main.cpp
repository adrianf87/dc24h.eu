#include "dc24h/adcs/tls.hpp"
#include "dc24h/accounts/adc_authentication.hpp"
#include "dc24h/configuration/configuration.hpp"
#include "dc24h/core/server.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/monitoring/process.hpp"
#include "dc24h/network/tcp_server.hpp"
#include "dc24h/version.hpp"
#include "dc24h/bans/repository.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
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
        if (key == "--help" || key == "--version" || key == "--check-config" ||
            key == "--check-database" || key == "--migrate-database" || key == "--database-status" || key == "--show-configuration") { continue; }
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
        std::cout << "dc24h.eu-twilight " << dc24h::version << '\n';
        return EXIT_SUCCESS;
    }
    if (parsed.show_configuration) {
        std::cout << dc24h::api::configuration_json(dc24h::configuration::public_settings(parsed.settings)) << '\n';
        return EXIT_SUCCESS;
    }
    if (parsed.check_only) {
        try {
            if (parsed.settings.authentication.enabled) {
                (void)dc24h::accounts::AdcSecretKey::load(parsed.settings.authentication.key_file);
            }
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

    if (parsed.check_database || parsed.migrate_database || parsed.database_status ||
        !parsed.backup_database.empty() || !parsed.restore_database.empty()) {
        try {
            dc24h::database::Connection database(parsed.settings.database);
            database.ping();
            if (parsed.migrate_database) {
                const auto applied = dc24h::database::migrate(database);
                std::cout << dc24h::i18n::translate("Database migrations applied=", language) << applied << '\n';
            } else if (!parsed.backup_database.empty()) {
                const auto report = dc24h::database::backup(database, parsed.backup_database);
                std::cout << dc24h::i18n::translate("Database backup written bytes=", language) << report.bytes << '\n';
            } else if (!parsed.restore_database.empty()) {
                const auto report = dc24h::database::restore(database, parsed.restore_database);
                std::cout << dc24h::i18n::translate("Database backup restored rows=", language) << report.rows << '\n';
            } else if (parsed.database_status) {
                const auto status = dc24h::database::inspect_migrations(database);
                std::cout << dc24h::i18n::translate("Database schema version=", language) << status.current_version
                          << " latest=" << status.latest_version << " pending=" << status.pending
                          << " dirty=" << (status.dirty ? "yes" : "no") << '\n';
                return status.dirty ? EXIT_FAILURE : EXIT_SUCCESS;
            } else {
                std::cout << dc24h::i18n::translate("Database connection OK\n", language);
            }
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << dc24h::i18n::translate(error.what(), language) << '\n';
            return EXIT_FAILURE;
        }
    }

    try {
        // Block termination signals before the asynchronous logging worker
        // is created so it inherits the same mask as every later thread.
        const dc24h::core::TerminationSignals signals;
        dc24h::logging::Logger logger(parsed.settings.logging);
        try {
            logger.info(std::string("Starting dc24h.eu-twilight ") + std::string(dc24h::version));
            logger.info("Startup settings: adc_listeners=" + std::to_string(parsed.settings.network.listeners.size()) +
                    " adcs_listeners=" + std::to_string(parsed.settings.network.tls_listeners.size()) +
                    " max_connections=" + std::to_string(parsed.settings.network.max_connections) +
                    " max_sessions=" + std::to_string(parsed.settings.network.users.bounded_by(parsed.settings.network.max_connections).max_sessions) +
                    " max_users=" + std::to_string(parsed.settings.network.users.bounded_by(parsed.settings.network.max_connections).max_users) +
                    " idle_timeout_seconds=" + std::to_string(parsed.settings.network.idle_timeout.count()) +
                    " connection_logging=" + (parsed.settings.logging.connections ? "on" : "off") +
                    " max_per_ip=" + std::to_string(parsed.settings.network.security.max_connections_per_ip) +
                    " accepts_per_minute=" + std::to_string(parsed.settings.network.security.max_accepts_per_minute) +
                    " frames_per_second=" + std::to_string(parsed.settings.network.security.max_frames_per_second) +
                    " max_invalid_frames=" + std::to_string(parsed.settings.network.security.max_invalid_frames) +
                    " max_pending_logins=" + std::to_string(parsed.settings.network.security.max_pending_logins) +
                    " max_pending_per_ip=" + std::to_string(parsed.settings.network.security.max_pending_per_ip) +
                    " max_login_bytes=" + std::to_string(parsed.settings.network.security.max_login_bytes) +
                    " max_login_frames=" + std::to_string(parsed.settings.network.security.max_login_frames) +
                    " max_identity_checks_per_minute=" + std::to_string(parsed.settings.network.security.max_identity_checks_per_minute) +
                    " max_identity_checks_per_second=" + std::to_string(parsed.settings.network.security.max_identity_checks_per_second) +
                    " tls_handshake_timeout_seconds=" + std::to_string(parsed.settings.network.security.tls_handshake_timeout.count()) +
                    " partial_frame_timeout_seconds=" + std::to_string(parsed.settings.network.security.partial_frame_timeout.count()) +
                    " login_timeout_seconds=" + std::to_string(parsed.settings.network.security.login_timeout.count()) +
                    " output_timeout_seconds=" + std::to_string(parsed.settings.network.security.output_timeout.count()) +
                    " close_timeout_seconds=" + std::to_string(parsed.settings.network.security.close_timeout.count()));
            std::unique_ptr<dc24h::database::Connection> database;
            if (parsed.settings.database.enabled) {
                database = std::make_unique<dc24h::database::Connection>(parsed.settings.database);
                database->ping();
                logger.info("Database connected");
                parsed.settings.network.bans=dc24h::bans::Repository(*database).load(dc24h::bans::Directory::now());
                logger.info("Ban policy loaded");
            }
            if (parsed.settings.authentication.enabled) {
                const auto key = dc24h::accounts::AdcSecretKey::load(parsed.settings.authentication.key_file);
                parsed.settings.network.authentication = dc24h::accounts::AdcAuthenticationDirectory::load(*database,key,
                    parsed.settings.authentication.max_accounts,parsed.settings.authentication.require_tls);
                logger.info("ADC account authentication enabled");
            }
            // Capture owning startup policy before Options moves its strings.
            const auto public_settings = dc24h::configuration::public_settings(parsed.settings);
            const auto transfer_settings = parsed.settings.network.transfers;
            dc24h::network::TcpServer network(logger, std::move(parsed.settings.network));
            dc24h::core::Server server(logger, &network);
            std::atomic<bool> signal_error{false};
            const auto started = std::chrono::steady_clock::now();
            std::unique_ptr<dc24h::api::Server> api;
            if (parsed.settings.api.enabled) {
                struct CpuWindow {
                    bool have{false};
                    std::uint64_t cpu_ms{0};
                    std::chrono::steady_clock::time_point at{};
                };
                api = std::make_unique<dc24h::api::Server>(logger, parsed.settings.api,
                    [&, window = CpuWindow{}]() mutable {
                    const auto process = dc24h::monitoring::read_process();
                    std::uint64_t window_ms = 0;
                    std::uint64_t delta_ms = 0;
                    if (!process.available) {
                        window = {};
                    } else {
                        const auto now = std::chrono::steady_clock::now();
                        const auto total = process.cpu_user_ms >
                            std::numeric_limits<std::uint64_t>::max() - process.cpu_system_ms
                            ? std::numeric_limits<std::uint64_t>::max()
                            : process.cpu_user_ms + process.cpu_system_ms;
                        if (!window.have || now < window.at || total < window.cpu_ms) {
                            window = CpuWindow{true, total, now};
                        } else {
                            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - window.at).count();
                            if (elapsed > 0) {
                                window_ms = static_cast<std::uint64_t>(elapsed);
                                delta_ms = total - window.cpu_ms;
                                window.cpu_ms = total;
                                window.at = now;
                            }
                        }
                    }
                    const auto stats = network.statistics();
                    return dc24h::api::Snapshot{
                        server.state() == dc24h::core::State::running,
                        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - started).count()),
                        stats.active, stats.accepted, stats.adc_frames, stats.adcs_handshakes,
                        stats.security_rejected, stats.security_closed, database != nullptr,
                        stats.rejected, stats.closed, stats.received_bytes, stats.sent_bytes,
                        stats.peak_connections, stats.online_users, stats.peak_online_users, stats.successful_logins, stats.chat, stats.search, parsed.settings.network.search, stats.transfers, transfer_settings, public_settings, stats.kick, stats.mute,
                        process.available, process.cpu_user_ms, process.cpu_system_ms, window_ms, delta_ms, process.rss_bytes,
                        stats.sessions_accepted, stats.sessions_negotiating, stats.sessions_identified,
                        stats.sessions_ready, stats.sessions_closing, stats.sessions_closed,
                        stats.sessions_created, stats.sessions_completed,
                        stats.login_samples, stats.login_last_ms, stats.login_max_ms,
                        stats.reactor_samples, stats.reactor_last_ms, stats.reactor_max_ms,
                        network.listeners_ready()};
                }, [&] { signal_error.store(true); server.request_stop(); }, dc24h::api::AccountHandler{}, [&] { return network.presence(); });
                api->start();
            }


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
            api.reset(); // Stop/join callbacks before Core, Network and database destruction.
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
