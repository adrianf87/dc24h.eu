#include "server_cli.hpp"

#include "database_cli.hpp"
#include "dc24h/db/store.hpp"
#include "dc24h/net/server.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <atomic>
#include <charconv>
#include <csignal>
#include <limits>
#include <ostream>
#include <set>
#include <stdexcept>
#include <syncstream>
#include <thread>
#include <utility>
#include <vector>

namespace dc24h {
namespace {

namespace asio = boost::asio;

struct LaunchConfig {
    net::ServerConfig server;
    std::size_t threads = 2;
    bool mariadb = false;
    std::set<std::string_view> setting_overrides;
};

std::uint64_t number(std::string_view value, std::uint64_t maximum, std::string_view option) {
    std::uint64_t parsed = 0;
    const auto end = value.data() + value.size();
    const auto result = std::from_chars(value.data(), end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed > maximum) {
        throw std::invalid_argument("Invalid numeric value for " + std::string(option));
    }
    return parsed;
}

LaunchConfig parse_options(std::span<const std::string_view> arguments) {
    LaunchConfig launch;
    net::Endpoint adc{"127.0.0.1", 1510};
    std::set<std::string_view> seen;
    constexpr auto max_port = std::numeric_limits<std::uint16_t>::max();

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto option = arguments[index];
        if (!seen.insert(option).second) {
            throw std::invalid_argument("Repeated option " + std::string(option));
        }
        if (option == "--enable-adc") {
            continue;
        }
        if (option == "--mariadb") {
            launch.mariadb = true;
            continue;
        }
        if (++index == arguments.size() ||
            (arguments[index].empty() && option != "--hub-description") ||
            arguments[index].starts_with("--")) {
            throw std::invalid_argument("Expected a value for " + std::string(option));
        }
        const auto value = arguments[index];
        if (option == "--tls-cert") {
            launch.server.certificate = value;
        } else if (option == "--tls-key") {
            launch.server.private_key = value;
        } else if (option == "--adcs-address") {
            launch.server.adcs.address = value;
        } else if (option == "--adcs-port") {
            launch.server.adcs.port = static_cast<std::uint16_t>(number(value, max_port, option));
        } else if (option == "--adc-address") {
            adc.address = value;
        } else if (option == "--adc-port") {
            adc.port = static_cast<std::uint16_t>(number(value, max_port, option));
        } else if (option == "--threads") {
            launch.threads = static_cast<std::size_t>(number(value, 64, option));
            if (launch.threads == 0) {
                throw std::invalid_argument("At least one I/O worker is required");
            }
        } else if (option.starts_with("--") &&
                   net::apply_hub_setting(launch.server, option.substr(2), value)) {
            launch.setting_overrides.emplace(option.substr(2));
        } else {
            throw std::invalid_argument("Unknown server option " + std::string(option));
        }
    }
    if (seen.contains("--enable-adc")) {
        launch.server.adc = std::move(adc);
    } else if (seen.contains("--adc-address") || seen.contains("--adc-port")) {
        throw std::invalid_argument("ADC address and port require --enable-adc");
    }
    return launch;
}

void load_settings(LaunchConfig &launch, const db::Config &config, std::ostream &errors) {
    try {
        db::Store store(config);
        store.require_current_schema();
        for (const auto &[name, value] : store.settings()) {
            if (launch.setting_overrides.contains(name)) {
                continue;
            }
            try {
                if (!net::apply_hub_setting(launch.server, name, value)) {
                    errors << "Ignoring an unknown MariaDB hub setting\n";
                }
            } catch (const std::invalid_argument &) {
                errors << "Ignoring an invalid MariaDB hub setting\n";
            }
        }
    } catch (const db::Error &error) {
        errors << "Using command-line settings and defaults at startup: " << error.what() << '\n';
    }
}

void print_listener(std::ostream &output, std::string_view protocol,
                    const net::Endpoint &endpoint) {
    output << "Listening for " << protocol << " on " << endpoint.address << " port "
           << endpoint.port << '\n';
}

int serve(LaunchConfig launch, std::ostream &output, std::ostream &errors) {
    std::optional<db::Config> database;
    if (launch.mariadb) {
        database.emplace(read_database_config());
        load_settings(launch, *database, errors);
    }
    asio::io_context io;
    net::Server server(
        io, std::move(launch.server),
        [&errors](std::string_view event) { std::osyncstream(errors) << event << '\n'; },
        std::move(database));
    asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&server](const boost::system::error_code &error, int) {
        if (!error) {
            server.stop();
        }
    });

    print_listener(output, "ADCS", server.adcs_endpoint());
    if (const auto &adc = server.adc_endpoint()) {
        print_listener(output, "ADC", *adc);
    }
    output.flush();

    std::atomic<bool> failed = false;
    auto work = [&io, &errors, &failed] {
        try {
            io.run();
        } catch (const std::exception &error) {
            failed.store(true);
            io.stop();
            std::osyncstream(errors) << "I/O worker failed: " << error.what() << '\n';
        }
    };
    std::vector<std::jthread> workers;
    workers.reserve(launch.threads - 1);
    try {
        for (std::size_t index = 1; index < launch.threads; ++index) {
            workers.emplace_back(work);
        }
    } catch (...) {
        io.stop();
        throw;
    }
    work();
    for (auto &worker : workers) {
        worker.join();
    }
    return failed.load() ? 1 : 0;
}

} // namespace

void write_server_help(std::ostream &output) {
    output << "       dc24h-hub --serve --tls-cert FILE --tls-key FILE [OPTIONS]\n"
           << "  --serve                  Start the ADC/ADCS server.\n"
           << "  --tls-cert FILE           ADCS PEM certificate chain.\n"
           << "  --tls-key FILE            Unencrypted ADCS PEM private key.\n"
           << "  --adcs-address IP         ADCS listen address (127.0.0.1).\n"
           << "  --adcs-port PORT          ADCS TCP port (1511).\n"
           << "  --enable-adc              Enable plaintext ADC.\n"
           << "  --adc-address IP          ADC listen address (127.0.0.1).\n"
           << "  --adc-port PORT           ADC TCP port (1510).\n"
           << "  --mariadb                 Load settings and enforce bans from MariaDB.\n"
           << "  --hub-name NAME           Advertised hub name (dc24h.eu).\n"
           << "  --hub-description TEXT    Advertised hub description.\n"
           << "  --max-clients COUNT       Concurrent clients across both listeners (128).\n"
           << "  --max-nick-codepoints N   Maximum Unicode code points in a nickname (32).\n"
           << "  --max-message-bytes N     Maximum decoded UTF-8 chat bytes (1024).\n"
           << "  --input-bytes BYTES       Maximum ADC frame bytes before LF (16384).\n"
           << "  --pending-bytes BYTES     Maximum queued output bytes per client (65536).\n"
           << "  --setup-timeout-ms MS     Overall connection setup deadline (30000).\n"
           << "  --tls-timeout-ms MS       TLS handshake deadline (10000).\n"
           << "  --identify-timeout-ms MS  ADC handshake deadline (15000).\n"
           << "  --idle-timeout-ms MS      Read inactivity deadline in NORMAL (300000).\n"
           << "  --shutdown-timeout-ms MS  Graceful close deadline per client (2000).\n"
           << "  --threads COUNT           I/O worker count (2).\n";
}

int run_server(std::span<const std::string_view> arguments, std::ostream &output,
               std::ostream &errors) {
    if (arguments.size() == 1 && arguments.front() == "--help") {
        output << "Usage:\n";
        write_server_help(output);
        return 0;
    }
    try {
        return serve(parse_options(arguments), output, errors);
    } catch (const std::invalid_argument &error) {
        errors << "dc24h-hub: " << error.what() << ". Try 'dc24h-hub --serve --help'.\n";
        return 2;
    } catch (const std::exception &error) {
        errors << "dc24h-hub: server startup failed: " << error.what() << '\n';
        return 1;
    }
}

} // namespace dc24h
