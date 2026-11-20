#pragma once

#include "dc24h/logging/logger.hpp"
#include "dc24h/database/database.hpp"
#include "dc24h/api/server.hpp"
#include "dc24h/network/tcp_server.hpp"

#include <string>
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace dc24h::configuration {
struct AuthenticationSettings {
    bool enabled{false};
    std::string key_file;
    std::size_t max_accounts{1024};
    bool require_tls{true};
};

struct Settings {
    network::Options network;
    logging::Options logging;
    database::Options database;
    api::Options api;
    AuthenticationSettings authentication;
    // Parsing provenance, used to preserve TLS-only CLI defaults and protect the INI.
    bool implicit_plain_listener{true};
    std::string source_file;
};

struct CommandLine {
    Settings settings;
    bool check_only{false};
    bool check_database{false};
    bool migrate_database{false};
    bool database_status{false};
    std::string backup_database;
    std::string restore_database;
    bool show_configuration{false};
    bool show_help{false};
    bool show_version{false};
};

// Bounded UTF-8 INI syntax parsing, no environment expansion or includes.
// Apply validate() after CLI overrides for cross-field validity.
[[nodiscard]] Settings load_file(const std::string& filename);
// Load --config first, then apply CLI overrides in argument order.
[[nodiscard]] CommandLine parse_arguments(int argc, char* const argv[]);
void validate(const Settings& settings);
// Validates settings but never reads credentials, connects SQL, binds or logs.
[[nodiscard]] api::ConfigurationSnapshot public_settings(const Settings& settings, bool maintenance = false);

// Read-only maintenance configuration review. No passwords, private paths,
// file bytes or arbitrary INI keys are exposed. The active values belong to
// the maintenance process, not to a separately running hub.
struct ReviewOption { std::string key; std::string value; };
using ReviewOptions = std::array<ReviewOption, 4>;
struct ReviewSnapshot {
    std::string revision;
    ReviewOptions saved;
    ReviewOptions active;
    bool matches_active{};
};
class StaleReview : public std::runtime_error {
public:
    StaleReview() : std::runtime_error("Stale configuration revision") {}
};
class WriteOutcomeUnknown : public std::runtime_error {
public:
    WriteOutcomeUnknown() : std::runtime_error("Configuration file outcome requires operator inspection") {}
};
class Review final {
public:
    explicit Review(const Settings& active);
    ~Review();
    Review(const Review&) = delete;
    Review& operator=(const Review&) = delete;
    [[nodiscard]] ReviewSnapshot read() const;
    // Validates a bounded set of allowlisted changes in memory; never writes.
    // Throws StaleReview if the file changed after the caller's read.
    [[nodiscard]] bool validate_candidate(std::string_view revision,
        const std::vector<std::pair<std::string, std::string>>& changes) const;
    // Both operations use a fixed local INI path, exclusive advisory lock,
    // per-file revision and atomic same-directory rename. Never accept paths.
    // A durable sidecar backup is written before the primary replacement.
    [[nodiscard]] std::string write_candidate(std::string_view revision,
        const std::vector<std::pair<std::string, std::string>>& changes) const;
    [[nodiscard]] std::string restore_backup(std::string_view revision) const;
private:
    std::string filename_;
    ReviewOptions active_;
    std::array<unsigned char, 32> revision_key_{};
    [[nodiscard]] std::string revision_for(std::string_view file) const;
};

} // namespace dc24h::configuration
