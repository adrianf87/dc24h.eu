#pragma once

#include "dc24h/logging/logger.hpp"
#include "dc24h/database/database.hpp"
#include "dc24h/network/tcp_server.hpp"

#include <string>

namespace dc24h::configuration {

struct Settings {
    network::Options network;
    logging::Options logging;
    database::Options database;
    // Parsing provenance, used to preserve TLS-only CLI defaults and protect the INI.
    bool implicit_plain_listener{true};
    std::string source_file;
};

struct CommandLine {
    Settings settings;
    bool check_only{false};
    bool check_database{false};
    bool migrate_database{false};
    bool show_help{false};
    bool show_version{false};
};

// Bounded UTF-8 INI syntax parsing, no environment expansion or includes.
// Apply validate() after CLI overrides for cross-field validity.
[[nodiscard]] Settings load_file(const std::string& filename);
// Load --config first, then apply CLI overrides in argument order.
[[nodiscard]] CommandLine parse_arguments(int argc, char* const argv[]);
void validate(const Settings& settings);

} // namespace dc24h::configuration
