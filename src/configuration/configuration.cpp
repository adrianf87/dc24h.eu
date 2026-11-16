#include "dc24h/configuration/configuration.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utf8proc.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

namespace dc24h::configuration {
namespace {
namespace fs = std::filesystem;
constexpr std::size_t max_file_bytes = 65536;
constexpr std::size_t max_line_bytes = 4096;
constexpr std::size_t max_welcome_line_bytes = 8192;

std::string trim(std::string_view value) {
    // INI whitespace is deliberately ASCII and independent of the process locale.
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) { value.remove_prefix(1); }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) { value.remove_suffix(1); }
    return std::string(value);
}

unsigned int positive_number(std::string_view value, unsigned int maximum) {
    unsigned int parsed{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || parsed == 0 || parsed > maximum) {
        throw std::invalid_argument("Invalid numeric configuration value");
    }
    return parsed;
}

logging::Level level(std::string_view value) {
    if (value == "info") { return logging::Level::info; }
    if (value == "warning") { return logging::Level::warning; }
    if (value == "error") { return logging::Level::error; }
    if (value == "off") { return logging::Level::off; }
    throw std::invalid_argument("Log level must be info, warning, error or off");
}

bool connection_logging(std::string_view value) {
    if (value == "on") { return true; }
    if (value == "off") { return false; }
    throw std::invalid_argument("Connection logging must be on or off");
}
bool transfer_switch(std::string_view value) {
    if (value == "on") { return true; }
    if (value == "off") { return false; }
    throw std::invalid_argument("Transfer switches must be on or off");
}

void path_text(std::string_view value) {
    if (value.empty()) { throw std::invalid_argument("Empty configuration path"); }
    for (const unsigned char c : value) {
        if (c < 32 || c == 127) { throw std::invalid_argument("Invalid control character in path"); }
    }
}

std::string resolve_path(std::string_view value, const fs::path& base) {
    path_text(value);
    const fs::path path(value);
    // Do not lexically collapse symlink/.. before filesystem resolution.
    return (path.is_absolute() ? path : base / path).string();
}

bool same_file(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    if (fs::equivalent(a, b, ec) && !ec) { return true; }
    ec.clear();
    const auto first = fs::weakly_canonical(a, ec);
    if (ec) { throw std::invalid_argument("Cannot resolve configured path"); }
    const auto second = fs::weakly_canonical(b, ec);
    if (ec) { throw std::invalid_argument("Cannot resolve configured path"); }
    return first == second;
}

struct ListenerList {
    bool seen{false};
    bool disabled{false};
    void add(Settings& settings, std::vector<network::Endpoint>& listeners, std::string_view value) {
        if (settings.implicit_plain_listener) {
            settings.network.listeners.clear();
            settings.implicit_plain_listener = false;
        }
        if (!seen) { listeners.clear(); seen = true; }
        if (value == "none") {
            if (!listeners.empty() || disabled) {
                throw std::invalid_argument("none cannot be combined with listener addresses");
            }
            disabled = true;
        } else {
            if (disabled || listeners.size() == 16) { throw std::invalid_argument("Invalid listener list"); }
            listeners.push_back(network::parse_endpoint(value));
        }
    }
};

void validate_endpoints(const std::vector<network::Endpoint>& list, std::unordered_set<std::string>& seen) {
    for (const auto& endpoint : list) {
        const bool ipv6 = endpoint.address.find(':') != std::string::npos;
        (void)network::parse_endpoint((ipv6 ? "[" + endpoint.address + "]" : endpoint.address)
                                     + ":" + std::to_string(endpoint.port));
        std::array<unsigned char, 16> address{};
        std::array<char, INET6_ADDRSTRLEN> normalized{};
        const int family = ipv6 ? AF_INET6 : AF_INET;
        if (::inet_pton(family, endpoint.address.c_str(), address.data()) != 1 ||
            ::inet_ntop(family, address.data(), normalized.data(), normalized.size()) == nullptr) {
            throw std::invalid_argument("Invalid listener address");
        }
        const auto key = std::string(normalized.data()) + ":" + std::to_string(endpoint.port);
        // Port zero requests independent ephemeral sockets, so it may be repeated.
        if (endpoint.port != 0 && !seen.insert(key).second) {
            throw std::invalid_argument("Duplicate listener endpoint");
        }
    }
}

std::string read_file(const std::string& filename) {
    path_text(filename);
    const int fd = ::open(filename.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) { throw std::invalid_argument("Cannot open configuration file (regular file required, no symlink)"); }
    struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
    struct stat state{};
    if (::fstat(fd, &state) != 0 || !S_ISREG(state.st_mode) || state.st_size < 0 ||
        static_cast<std::uintmax_t>(state.st_size) > max_file_bytes) {
        throw std::invalid_argument("Configuration must be a regular file of at most 64 KiB");
    }
    std::array<char, 4096> buffer{};
    std::string data;
    for (;;) {
        const auto count = ::read(fd, buffer.data(), buffer.size());
        if (count < 0 && errno == EINTR) { continue; }
        if (count < 0) { throw std::invalid_argument("Configuration read failure"); }
        if (count == 0) { break; }
        if (data.size() + static_cast<std::size_t>(count) > max_file_bytes) {
            throw std::invalid_argument("Configuration file exceeds 64 KiB");
        }
        data.append(buffer.data(), static_cast<std::size_t>(count));
    }
    for (std::size_t i = 0; i < data.size();) {
        utf8proc_int32_t codepoint{};
        const auto count = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(data.data() + i),
                                            static_cast<utf8proc_ssize_t>(data.size() - i), &codepoint);
        if (count <= 0 || (codepoint < 32 && codepoint != '\t' && codepoint != '\n' && codepoint != '\r') ||
            codepoint == 127) {
            throw std::invalid_argument("Configuration must be UTF-8 without control characters");
        }
        i += static_cast<std::size_t>(count);
    }
    return data;
}
} // namespace

void validate(const Settings& settings) {
    database::validate(settings.database);
    api::validate(settings.api);
    const auto& net = settings.network;
    if (settings.authentication.enabled && (!settings.database.enabled || settings.authentication.key_file.empty() ||
        settings.authentication.max_accounts == 0 || settings.authentication.max_accounts > 4096 ||
        (settings.authentication.require_tls && net.tls_listeners.empty()))) {
        throw std::invalid_argument("ADC authentication requires database, private key and compatible listeners");
    }
    if (!settings.authentication.key_file.empty()) {
        path_text(settings.authentication.key_file);
        for (const auto& other : {settings.database.password_file,settings.api.token_file,settings.source_file,net.tls_private_key,net.tls_certificate}) {
            if (!other.empty() && same_file(settings.authentication.key_file,other)) {
                throw std::invalid_argument("ADC authentication key must use a separate private file");
            }
        }
    }
    security::validate(net.security);
    security::validate(net.authentication_limits);
    users::validate(net.users);
    chat::validate_settings(net.chat);
    search::validate_security(net.search);
    transfers::validate_limits(net.transfers.flood);
    const auto count = net.listeners.size() + net.tls_listeners.size();
    if (count == 0 || count > 16 || net.max_connections == 0 || net.max_connections > 65535 ||
        net.idle_timeout.count() < 1 || net.idle_timeout.count() > 86400) {
        throw std::invalid_argument("Invalid listener or connection limits");
    }
    const bool tls = !net.tls_listeners.empty();
    if (tls != (!net.tls_certificate.empty() && !net.tls_private_key.empty()) ||
        (!tls && (!net.tls_certificate.empty() || !net.tls_private_key.empty()))) {
        throw std::invalid_argument("ADCS requires listeners, a certificate and a private key");
    }
    std::unordered_set<std::string> endpoints;
    validate_endpoints(net.listeners, endpoints);
    validate_endpoints(net.tls_listeners, endpoints);
    if (tls) { path_text(net.tls_certificate); path_text(net.tls_private_key); }
    if (!settings.source_file.empty()) { path_text(settings.source_file); }
    if (!settings.api.socket_path.empty()) {
        for (const auto& protected_path : {settings.source_file, net.tls_certificate, net.tls_private_key,
                settings.database.password_file, settings.database.socket, settings.api.token_file, settings.logging.file_path, settings.authentication.key_file}) {
            if (!protected_path.empty() && same_file(settings.api.socket_path, protected_path)) {
                throw std::invalid_argument("API socket cannot target configuration or credential files");
            }
        }
    }
    const auto& log = settings.logging;
    if (log.level != logging::Level::info && log.level != logging::Level::warning && log.level != logging::Level::error && log.level != logging::Level::off) {
        throw std::invalid_argument("Invalid log level");
    }
    if (log.language != i18n::Language::us && log.language != i18n::Language::pl) {
        throw std::invalid_argument("Unknown language (use us or pl)");
    }
    if (!log.file_path.empty()) {
        path_text(log.file_path);
        for (const auto& protected_path : {settings.source_file, net.tls_certificate, net.tls_private_key, settings.database.password_file, settings.api.token_file, settings.api.socket_path, settings.authentication.key_file}) {
            if (!protected_path.empty() && same_file(log.file_path, protected_path)) {
                throw std::invalid_argument("Log file cannot target configuration or credential files");
            }
        }
    }
}

Settings load_file(const std::string& filename) {
    const auto data = read_file(filename);
    std::error_code ec;
    const auto path = fs::canonical(filename, ec);
    if (ec) { throw std::invalid_argument("Cannot resolve configuration file path"); }
    Settings settings;
    settings.source_file = path.string();
    std::istringstream input(data);
    std::string section;
    std::unordered_set<std::string> singletons;
    ListenerList plain, tls;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.size() > max_line_bytes) {
            // A maximum-size welcome also needs room for its key, whitespace
            // and CR. Other physical lines retain their existing bound.
            const auto equal = line.find('=');
            if (line.size() > max_welcome_line_bytes || section != "chat" || equal == std::string::npos ||
                trim(std::string_view(line).substr(0, equal)) != "welcome_message") {
                throw std::invalid_argument("Invalid configuration line " + std::to_string(line_number));
            }
        }
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        if (line.find('\r') != std::string::npos) {
            throw std::invalid_argument("Invalid configuration line " + std::to_string(line_number));
        }
        if (line_number == 1 && line.starts_with("\xEF\xBB\xBF")) { line.erase(0, 3); }
        const auto text = trim(line);
        if (text.empty() || text.front() == '#' || text.front() == ';') { continue; }
        if (text.front() == '[') {
            if (text.back() != ']' || text.size() < 3) {
                throw std::invalid_argument("Invalid section at line " + std::to_string(line_number));
            }
            section = trim(std::string_view(text).substr(1, text.size() - 2));
            if (section != "network" && section != "tls" && section != "logging" && section != "general" && section != "security" && section != "database" && section != "api" && section != "users" && section != "chat" && section != "search" && section != "transfers" && section != "authentication") {
                throw std::invalid_argument("Unknown configuration section at line " + std::to_string(line_number));
            }
            continue;
        }
        const auto equal = text.find('=');
        if (equal == std::string::npos || section.empty()) {
            throw std::invalid_argument("Expected section and key=value at line " + std::to_string(line_number));
        }
        const auto key = trim(std::string_view(text).substr(0, equal));
        const auto value = trim(std::string_view(text).substr(equal + 1));
        if (!(section == "network" && (key == "listen" || key == "adcs_listen")) &&
            !singletons.insert(section + "." + key).second) {
            throw std::invalid_argument("Duplicate configuration key at line " + std::to_string(line_number));
        }
        try {
            if (section == "network" && key == "listen") {
                plain.add(settings, settings.network.listeners, value);
            } else if (section == "network" && key == "adcs_listen") {
                tls.add(settings, settings.network.tls_listeners, value);
            } else if (section == "network" && key == "max_connections") {
                settings.network.max_connections = positive_number(value, 65535);
            } else if (section == "network" && key == "idle_timeout") {
                settings.network.idle_timeout = std::chrono::seconds(positive_number(value, 86400));
            } else if (section == "users" && key == "max_sessions") {
                settings.network.users.max_sessions = positive_number(value, 65535);
            } else if (section == "users" && key == "max_users") {
                settings.network.users.max_users = positive_number(value, 65535);
            } else if (section == "chat" && key == "max_messages_per_second") {
                settings.network.chat.flood.messages_per_second = positive_number(value, chat::max_message_rate);
            } else if (section == "chat" && key == "message_burst") {
                settings.network.chat.flood.message_burst = positive_number(value, chat::max_message_rate);
            } else if (section == "chat" && key == "max_bytes_per_second") {
                settings.network.chat.flood.bytes_per_second = positive_number(value, chat::max_byte_rate);
            } else if (section == "chat" && key == "byte_burst") {
                settings.network.chat.flood.byte_burst = positive_number(value, chat::max_byte_rate);
            } else if (section == "chat" && key == "max_text_bytes") {
                settings.network.chat.policy.max_text_bytes = positive_number(value, text::max_chat_text_bytes);
            } else if (section == "chat" && key == "public_enabled") {
                settings.network.chat.policy.public_enabled = connection_logging(value);
            } else if (section == "chat" && key == "private_enabled") {
                settings.network.chat.policy.private_enabled = connection_logging(value);
            } else if (section == "chat" && key == "actions_enabled") {
                settings.network.chat.policy.actions_enabled = connection_logging(value);
            } else if (section == "chat" && key == "welcome_enabled") {
                settings.network.chat.welcome_enabled = connection_logging(value);
            } else if (section == "chat" && key == "welcome_message") {
                settings.network.chat.welcome_message = value;
            } else if (section == "transfers" && key == "allowed_protocols") {
                settings.network.transfers.allowed_protocols = transfers::ProtocolPolicy::parse(value);
            } else if (section == "transfers" && key == "ctm_enabled") {
                settings.network.transfers.ctm_enabled = transfer_switch(value);
            } else if (section == "transfers" && key == "rcm_enabled") {
                settings.network.transfers.rcm_enabled = transfer_switch(value);
            } else if (section == "transfers" && key == "guest_initiate") {
                settings.network.transfers.guests.initiate = transfer_switch(value);
            } else if (section == "transfers" && key == "guest_receive") {
                settings.network.transfers.guests.receive = transfer_switch(value);
            } else if (section == "transfers" && key == "request_rate") {
                settings.network.transfers.flood.requests.rate = positive_number(value, transfers::max_request_rate);
            } else if (section == "transfers" && key == "request_burst") {
                settings.network.transfers.flood.requests.burst = positive_number(value, transfers::max_request_rate);
            } else if (section == "transfers" && key == "request_byte_rate") {
                settings.network.transfers.flood.requests.byte_rate = positive_number(value, transfers::max_transfer_byte_rate);
            } else if (section == "transfers" && key == "request_byte_burst") {
                settings.network.transfers.flood.requests.byte_burst = positive_number(value, transfers::max_transfer_byte_rate);
            } else if (section == "transfers" && key == "incoming_rate") {
                settings.network.transfers.flood.incoming.rate = positive_number(value, transfers::max_request_rate);
            } else if (section == "transfers" && key == "incoming_burst") {
                settings.network.transfers.flood.incoming.burst = positive_number(value, transfers::max_request_rate);
            } else if (section == "transfers" && key == "incoming_byte_rate") {
                settings.network.transfers.flood.incoming.byte_rate = positive_number(value, transfers::max_transfer_byte_rate);
            } else if (section == "transfers" && key == "incoming_byte_burst") {
                settings.network.transfers.flood.incoming.byte_burst = positive_number(value, transfers::max_transfer_byte_rate);
            } else if (section == "search" && key == "queries_enabled") {
                settings.network.search.queries_enabled = connection_logging(value);
            } else if (section == "search" && key == "results_enabled") {
                settings.network.search.results_enabled = connection_logging(value);
            } else if (section == "search" && key == "guest_queries") {
                settings.network.search.guests.queries = connection_logging(value);
            } else if (section == "search" && key == "guest_results") {
                settings.network.search.guests.results = connection_logging(value);
            } else if (section == "search" && key == "require_result_token") {
                settings.network.search.require_result_token = connection_logging(value);
            } else if (section == "search" && key == "max_active_searches") {
                settings.network.search.tokens.total = positive_number(value, 65535);
            } else if (section == "search" && key == "max_active_per_user") {
                settings.network.search.tokens.per_owner = positive_number(value, 64);
            } else if (section == "search" && key == "search_lifetime_seconds") {
                settings.network.search.tokens.lifetime = std::chrono::seconds(positive_number(value, 3600));
            } else if (section == "search" && key == "query_rate") {
                settings.network.search.queries.rate = positive_number(value, search::max_operation_rate);
            } else if (section == "search" && key == "query_burst") {
                settings.network.search.queries.burst = positive_number(value, search::max_operation_rate);
            } else if (section == "search" && key == "query_byte_rate") {
                settings.network.search.queries.byte_rate = positive_number(value, search::max_security_byte_rate);
            } else if (section == "search" && key == "query_byte_burst") {
                settings.network.search.queries.byte_burst = positive_number(value, search::max_security_byte_rate);
            } else if (section == "search" && key == "result_rate") {
                settings.network.search.results.rate = positive_number(value, search::max_operation_rate);
            } else if (section == "search" && key == "result_burst") {
                settings.network.search.results.burst = positive_number(value, search::max_operation_rate);
            } else if (section == "search" && key == "result_byte_rate") {
                settings.network.search.results.byte_rate = positive_number(value, search::max_security_byte_rate);
            } else if (section == "search" && key == "result_byte_burst") {
                settings.network.search.results.byte_burst = positive_number(value, search::max_security_byte_rate);
            } else if (section == "security" && key == "max_connections_per_ip") {
                settings.network.security.max_connections_per_ip = positive_number(value, 65535);
            } else if (section == "security" && key == "max_accepts_per_minute") {
                settings.network.security.max_accepts_per_minute = positive_number(value, 100000);
            } else if (section == "security" && key == "max_frames_per_second") {
                settings.network.security.max_frames_per_second = positive_number(value, 100000);
            } else if (section == "security" && key == "max_invalid_frames") {
                settings.network.security.max_invalid_frames = positive_number(value, 65535);
            } else if (section == "security" && key == "max_pending_logins") {
                settings.network.security.max_pending_logins = positive_number(value, 65535);
            } else if (section == "security" && key == "max_pending_per_ip") {
                settings.network.security.max_pending_per_ip = positive_number(value, 65535);
            } else if (section == "security" && key == "max_login_bytes") {
                settings.network.security.max_login_bytes = positive_number(value, 16777216);
            } else if (section == "security" && key == "max_login_frames") {
                settings.network.security.max_login_frames = positive_number(value, 65535);
            } else if (section == "security" && key == "max_identity_checks_per_minute") {
                settings.network.security.max_identity_checks_per_minute = positive_number(value, 100000);
            } else if (section == "security" && key == "max_identity_checks_per_second") {
                settings.network.security.max_identity_checks_per_second = positive_number(value, 100000);
            } else if (section == "security" && key == "tls_handshake_timeout") {
                settings.network.security.tls_handshake_timeout = std::chrono::seconds(positive_number(value, 3600));
            } else if (section == "security" && key == "partial_frame_timeout") {
                settings.network.security.partial_frame_timeout = std::chrono::seconds(positive_number(value, 3600));
            } else if (section == "security" && key == "login_timeout") {
                settings.network.security.login_timeout = std::chrono::seconds(positive_number(value, 3600));
            } else if (section == "security" && key == "output_timeout") {
                settings.network.security.output_timeout = std::chrono::seconds(positive_number(value, 3600));
            } else if (section == "security" && key == "close_timeout") {
                settings.network.security.close_timeout = std::chrono::seconds(positive_number(value, 3600));
            } else if (section == "tls" && key == "certificate") {
                settings.network.tls_certificate = resolve_path(value, path.parent_path());
            } else if (section == "tls" && key == "private_key") {
                settings.network.tls_private_key = resolve_path(value, path.parent_path());
            } else if (section == "logging" && key == "level") {
                settings.logging.level = level(value);
            } else if (section == "logging" && key == "file") {
                settings.logging.file_path = value == "stderr" ? "" : resolve_path(value, path.parent_path());
            } else if (section == "logging" && key == "chat") {
                settings.logging.chat = connection_logging(value);
            } else if (section == "logging" && key == "connections") {
                settings.logging.connections = connection_logging(value);
            } else if (section == "api" && key == "enabled") {
                settings.api.enabled = connection_logging(value);
            } else if (section == "api" && key == "socket") {
                settings.api.socket_path = resolve_path(value, path.parent_path());
            } else if (section == "api" && key == "token_file") {
                settings.api.token_file = resolve_path(value, path.parent_path());
            } else if (section == "api" && key == "max_connections") {
                settings.api.max_connections = positive_number(value, 128);
            } else if (section == "api" && key == "request_timeout") {
                settings.api.request_timeout = positive_number(value, 30);
            } else if (section == "authentication" && key == "enabled") {
                settings.authentication.enabled = connection_logging(value);
            } else if (section == "authentication" && key == "key_file") {
                settings.authentication.key_file = resolve_path(value,path.parent_path());
            } else if (section == "authentication" && key == "max_accounts") {
                settings.authentication.max_accounts = positive_number(value,4096);
            } else if (section == "authentication" && key == "attempts_per_ip") {
                settings.network.authentication_limits.attempts_per_ip = positive_number(value,100000);
            } else if (section == "authentication" && key == "attempts_per_account") {
                settings.network.authentication_limits.attempts_per_account = positive_number(value,100000);
            } else if (section == "authentication" && key == "attempts_per_second") {
                settings.network.authentication_limits.attempts_per_second = positive_number(value,100000);
            } else if (section == "authentication" && key == "attempt_window_seconds") {
                settings.network.authentication_limits.window = std::chrono::seconds(positive_number(value,3600));
            } else if (section == "authentication" && key == "max_tracked_attempt_ips") {
                settings.network.authentication_limits.max_tracked_ips = positive_number(value,65536);
            } else if (section == "authentication" && key == "max_tracked_attempt_accounts") {
                settings.network.authentication_limits.max_tracked_accounts = positive_number(value,65536);
            } else if (section == "authentication" && key == "require_tls") {
                settings.authentication.require_tls = connection_logging(value);
            } else if (section == "database" && key == "enabled") {
                settings.database.enabled = connection_logging(value);
            } else if (section == "database" && key == "socket") {
                settings.database.socket = value.empty() ? "" : resolve_path(value, path.parent_path());
            } else if (section == "database" && key == "name") {
                settings.database.name = value;
            } else if (section == "database" && key == "user") {
                settings.database.user = value;
            } else if (section == "database" && key == "password_file") {
                settings.database.password_file = resolve_path(value, path.parent_path());
            } else if (section == "database" && key == "timeout") {
                settings.database.timeout = positive_number(value, 60);
            } else if (section == "general" && key == "language") {
                settings.logging.language = i18n::parse_language(value);
            } else { throw std::invalid_argument("Unknown configuration key"); }
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("Invalid or unknown configuration key/value at line " + std::to_string(line_number));
        }
    }
    return settings; // Cross-field validation follows CLI overrides.
}

CommandLine parse_arguments(int argc, char* const argv[]) {
    CommandLine result;
    std::string filename;
    bool has_config = false;
    bool operational = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view key(argv[i]);
        if (key == "--help") { result.show_help = true; continue; }
        if (key == "--version") { result.show_version = true; continue; }
        if (key == "--check-config" || key == "--check-database" || key == "--migrate-database" || key == "--database-status") { operational = true; continue; }
        if (key == "--config") {
            if (has_config || i + 1 == argc) {
                throw std::invalid_argument("Provide --config only once with a file");
            }
            filename = argv[++i]; has_config = true; operational = true; continue;
        }
        if (key == "--listen" || key == "--adcs-listen" || key == "--tls-cert" || key == "--tls-key" ||
            key == "--max-connections" || key == "--max-sessions" || key == "--max-users" || key == "--idle-timeout" || key == "--log-file" ||
            key == "--log-level" || key == "--log-chat" || key == "--log-connections" || key == "--language" ||
            key == "--max-per-ip" || key == "--accepts-per-minute" || key == "--frames-per-second" ||
            key == "--max-invalid-frames" || key == "--tls-handshake-timeout" || key == "--partial-frame-timeout") {
            if (i + 1 == argc) { throw std::invalid_argument("Missing option value"); }
            if (key != "--language") { operational = true; }
            ++i; continue;
        }
        throw std::invalid_argument("Unknown option");
    }
    if ((result.show_help || result.show_version) && (operational || (result.show_help && result.show_version))) {
        throw std::invalid_argument("Help/version cannot be combined with operational options");
    }
    if (has_config) { result.settings = load_file(filename); }
    ListenerList plain, tls;
    for (int i = 1; i < argc; ++i) {
        const std::string_view key(argv[i]);
        if (key == "--config") { ++i; continue; }
        if (key == "--check-config") { result.check_only = true; continue; }
        if (key == "--check-database") { result.check_database = true; continue; }
        if (key == "--migrate-database") { result.migrate_database = true; continue; }
        if (key == "--database-status") { result.database_status = true; continue; }
        if (key == "--help" || key == "--version") { continue; }
        const std::string_view value(argv[++i]);
        if (key == "--listen") { plain.add(result.settings, result.settings.network.listeners, value); }
        else if (key == "--adcs-listen") { tls.add(result.settings, result.settings.network.tls_listeners, value); }
        else if (key == "--tls-cert") {
            path_text(value); result.settings.network.tls_certificate = value;
        } else if (key == "--tls-key") {
            path_text(value); result.settings.network.tls_private_key = value;
        } else if (key == "--max-connections") {
            result.settings.network.max_connections = positive_number(value, 65535);
        } else if (key == "--max-sessions") {
            result.settings.network.users.max_sessions = positive_number(value, 65535);
        } else if (key == "--max-users") {
            result.settings.network.users.max_users = positive_number(value, 65535);
        } else if (key == "--idle-timeout") {
            result.settings.network.idle_timeout = std::chrono::seconds(positive_number(value, 86400));
        } else if (key == "--max-per-ip") {
            result.settings.network.security.max_connections_per_ip = positive_number(value, 65535);
        } else if (key == "--accepts-per-minute") {
            result.settings.network.security.max_accepts_per_minute = positive_number(value, 100000);
        } else if (key == "--frames-per-second") {
            result.settings.network.security.max_frames_per_second = positive_number(value, 100000);
        } else if (key == "--max-invalid-frames") {
            result.settings.network.security.max_invalid_frames = positive_number(value, 65535);
        } else if (key == "--tls-handshake-timeout") {
            result.settings.network.security.tls_handshake_timeout = std::chrono::seconds(positive_number(value, 3600));
        } else if (key == "--partial-frame-timeout") {
            result.settings.network.security.partial_frame_timeout = std::chrono::seconds(positive_number(value, 3600));
        } else if (key == "--log-level") { result.settings.logging.level = level(value); }
        else if (key == "--log-chat") { result.settings.logging.chat = connection_logging(value); }
        else if (key == "--log-connections") { result.settings.logging.connections = connection_logging(value); }
        else if (key == "--log-file") {
            path_text(value); result.settings.logging.file_path = value == "-" ? "" : std::string(value);
        } else if (key == "--language") { result.settings.logging.language = i18n::parse_language(value); }
    }
    if (static_cast<int>(result.check_only) + static_cast<int>(result.check_database) +
        static_cast<int>(result.migrate_database) + static_cast<int>(result.database_status) > 1) {
        throw std::invalid_argument("Database actions and configuration check are mutually exclusive");
    }
    if ((result.check_database || result.migrate_database || result.database_status) && !result.settings.database.enabled) {
        throw std::invalid_argument("Database is disabled");
    }
    validate(result.settings);
    return result;
}
} // namespace dc24h::configuration
