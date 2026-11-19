#include "dc24h/api/server.hpp"
#include "dc24h/api/http.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/security/secret_file.hpp"
#include "dc24h/version.hpp"

#include <openssl/crypto.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace dc24h::api {
namespace {
template <std::size_t N>
constexpr std::size_t named_index(const std::array<std::string_view, N>& names, std::string_view key) noexcept {
    for (std::size_t i = 0; i < N; ++i) {
        if (names[i] == key) { return i; }
    }
    return N;
}
constexpr auto chat_received_index = named_index(chat::statistic_names, "received");
constexpr auto chat_public_index = named_index(chat::statistic_names, "public_dispatches");
constexpr auto chat_private_index = named_index(chat::statistic_names, "private_dispatches");
constexpr auto chat_rejected_index = named_index(chat::statistic_names, "rejected");
constexpr auto chat_flood_index = named_index(chat::statistic_names, "flood_limited");
constexpr auto search_queries_index = named_index(search::statistic_names, "queries_received");
constexpr auto search_routed_index = named_index(search::statistic_names, "queries_routed");
constexpr auto search_rejected_index = named_index(search::statistic_names, "queries_rejected");
constexpr auto search_results_index = named_index(search::statistic_names, "results_received");
constexpr auto transfer_ctm_index = named_index(transfers::statistic_names, "ctm_received");
constexpr auto transfer_rcm_index = named_index(transfers::statistic_names, "rcm_received");
constexpr auto transfer_unsupported_index = named_index(transfers::statistic_names, "peer_protocol_unsupported");
constexpr auto transfer_failed_index = named_index(transfers::statistic_names, "peer_connection_failed");
constexpr auto kick_succeeded_index = named_index(admin::kick_statistic_names, "succeeded");
constexpr auto kick_refused_index = named_index(admin::kick_statistic_names, "refused");
constexpr auto mute_applied_index = named_index(admin::mute_statistic_names, "applied");
constexpr auto mute_public_index = named_index(admin::mute_statistic_names, "public_blocked");
constexpr auto mute_private_index = named_index(admin::mute_statistic_names, "private_blocked");
static_assert(chat_received_index < chat::statistic_names.size());
static_assert(chat_public_index < chat::statistic_names.size());
static_assert(chat_private_index < chat::statistic_names.size());
static_assert(chat_rejected_index < chat::statistic_names.size());
static_assert(chat_flood_index < chat::statistic_names.size());
static_assert(search_queries_index < search::statistic_names.size());
static_assert(search_routed_index < search::statistic_names.size());
static_assert(search_rejected_index < search::statistic_names.size());
static_assert(search_results_index < search::statistic_names.size());
static_assert(transfer_ctm_index < transfers::statistic_names.size());
static_assert(transfer_rcm_index < transfers::statistic_names.size());
static_assert(transfer_unsupported_index < transfers::statistic_names.size());
static_assert(transfer_failed_index < transfers::statistic_names.size());
static_assert(kick_succeeded_index < admin::kick_statistic_names.size());
static_assert(kick_refused_index < admin::kick_statistic_names.size());
static_assert(mute_applied_index < admin::mute_statistic_names.size());
static_assert(mute_public_index < admin::mute_statistic_names.size());
static_assert(mute_private_index < admin::mute_statistic_names.size());
}

std::string dashboard_json(const Snapshot& value) {
    std::string json{"{\"schema\":1,\"core_running\":"};
    json += value.core_running ? "true" : "false";
    json += ",\"database_enabled\":";
    json += value.database_enabled ? "true" : "false";
    const auto field = [&](std::string_view key, std::uint64_t count) {
        json += ",\"";
        json += key;
        json += "\":\"";
        json += std::to_string(count);
        json += '"';
    };
    field("uptime_seconds", value.uptime_seconds);
    field("users_online", value.online_users);
    field("users_peak", value.peak_online_users);
    field("users_logins", value.successful_logins);
    field("connections_active", value.active_connections);
    field("connections_peak", value.peak_connections);
    field("connections_accepted", value.accepted_connections);
    field("connections_rejected", value.rejected_connections);
    field("connections_closed", value.closed_connections);
    field("security_rejected", value.security_rejected);
    field("security_closed", value.security_closed);
    field("adc_frames", value.adc_frames);
    field("adcs_handshakes", value.adcs_handshakes);
    field("bytes_received", value.received_bytes);
    field("bytes_sent", value.sent_bytes);
    field("chat_received", value.chat.values[chat_received_index]);
    field("chat_public", value.chat.values[chat_public_index]);
    field("chat_private", value.chat.values[chat_private_index]);
    field("chat_rejected", value.chat.values[chat_rejected_index]);
    field("chat_flood_limited", value.chat.values[chat_flood_index]);
    field("search_queries", value.search.values[search_queries_index]);
    field("search_routed", value.search.values[search_routed_index]);
    field("search_rejected", value.search.values[search_rejected_index]);
    field("search_results", value.search.values[search_results_index]);
    field("search_tracked", value.search.tracked_searches);
    field("transfers_ctm", value.transfers.values[transfer_ctm_index]);
    field("transfers_rcm", value.transfers.values[transfer_rcm_index]);
    field("transfers_peer_unsupported", value.transfers.values[transfer_unsupported_index]);
    field("transfers_peer_failed", value.transfers.values[transfer_failed_index]);
    field("kicks_succeeded", value.kick.values[kick_succeeded_index]);
    field("kicks_refused", value.kick.values[kick_refused_index]);
    field("mutes_applied", value.mute.values[mute_applied_index]);
    field("mutes_public_blocked", value.mute.values[mute_public_index]);
    field("mutes_private_blocked", value.mute.values[mute_private_index]);
    json += '}';
    return json;
}

std::string users_json(const users::PresenceDirectory& directory) {
    auto escape = [](std::string_view text) {
        std::string value = "\"";
        for (const char value_byte : text) {
            const auto byte = static_cast<unsigned char>(value_byte);
            if (byte == '"' || byte == '\\') { value += '\\'; }
            value += value_byte;
        }
        return value + '"';
    };
    std::string rows = "[";
    bool truncated = directory.truncated;
    bool first = true;
    const auto cap = static_cast<std::uint32_t>(users::presence_capacity);
    const auto limit = directory.count < cap ? directory.count : cap;
    for (std::uint32_t index = 0; index < limit; ++index) {
        const auto& user = directory.users[index];
        if (user.nickname_len == 0 || static_cast<std::size_t>(user.nickname_len) > user.nickname.size()) { truncated = true; continue; }
        bool controls = false;
        for (std::uint8_t byte_index = 0; byte_index < user.nickname_len; ++byte_index) {
            const auto byte = static_cast<unsigned char>(user.nickname[byte_index]);
            controls = controls || byte < 0x20 || byte == 0x7f;
        }
        bool sid_ok = true;
        for (const char value : user.sid) {
            const auto byte = static_cast<unsigned char>(value);
            sid_ok = sid_ok && ((byte >= 'A' && byte <= 'Z') || (byte >= '2' && byte <= '7'));
        }
        const auto role = accounts::role_key(user.role);
        if (controls || !sid_ok || role.empty()) { truncated = true; continue; }
        const std::string row = std::string("{\"session\":\"") + std::to_string(user.session_id) +
            "\",\"sid\":" + escape(std::string_view(user.sid.data(), user.sid.size())) +
            ",\"nickname\":" + escape(std::string_view(user.nickname.data(), user.nickname_len)) +
            ",\"transport\":\"" + (user.transport == users::Transport::adcs ? "adcs" : "adc") +
            "\",\"role\":" + escape(role) +
            ",\"public_muted\":" + (user.public_muted ? "true" : "false") +
            ",\"private_muted\":" + (user.private_muted ? "true" : "false") + "}";
        if (rows.size() + row.size() > 11000) { truncated = true; break; }
        if (!first) { rows += ','; }
        first = false;
        rows += row;
    }
    rows += ']';
    return std::string("{\"schema\":1,\"truncated\":") + (truncated ? "true" : "false") + ",\"users\":" + rows + "}\n";
}

namespace {
using Clock = std::chrono::steady_clock;
struct Fd {
    int value{-1};
    explicit Fd(int fd = -1) noexcept : value(fd) {}
    ~Fd() { reset(); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : value(std::exchange(other.value, -1)) {}
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) { reset(); value = std::exchange(other.value, -1); }
        return *this;
    }
    void reset() noexcept { if (value >= 0) { ::close(value); value = -1; } }
};
struct Peer {
    Fd fd;
    std::string input;
    std::string output;
    std::size_t sent{0};
    Clock::time_point accepted{Clock::now()};
    explicit Peer(Fd socket) : fd(std::move(socket)) {}
    Peer(Peer&&) noexcept = default;
    Peer& operator=(Peer&& other) noexcept {
        if(this!=&other) {
            OPENSSL_cleanse(input.data(),input.size());fd=std::move(other.fd);input=std::move(other.input);
            output=std::move(other.output);sent=other.sent;accepted=other.accepted;
        }
        return *this;
    }
    ~Peer() { OPENSSL_cleanse(input.data(), input.size()); }
};
bool path_text(const std::string& value) {
    return value.size() <= 4096 && std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
std::string search_json(const Snapshot& value) {
    std::string json{"{\"statistics\":{"};
    for(std::size_t i=0;i<search::statistic_names.size();++i){
        if(i){json+=',';}json+='\"';json+=search::statistic_names[i];json+="\":";json+=std::to_string(value.search.values[i]);
    }
    json+=",\"tracked_searches\":"+std::to_string(value.search.tracked_searches)+"},\"configuration\":{";
    const auto& s=value.search_settings;
    const auto boolean=[&](std::string_view key,bool enabled){json+='\"';json+=key;json+=enabled?"\":true,":"\":false,";};
    const auto number=[&](std::string_view key,std::uint64_t count){json+='\"';json+=key;json+="\":";json+=std::to_string(count);json+=',';};
    boolean("queries_enabled",s.queries_enabled);boolean("results_enabled",s.results_enabled);
    boolean("guest_queries",s.guests.queries);boolean("guest_results",s.guests.results);boolean("require_result_token",s.require_result_token);
    number("max_active_searches",s.tokens.total);number("max_active_per_user",s.tokens.per_owner);
    number("search_lifetime_seconds",static_cast<std::uint64_t>(s.tokens.lifetime.count()));
    number("query_rate",s.queries.rate);number("query_burst",s.queries.burst);number("query_byte_rate",s.queries.byte_rate);number("query_byte_burst",s.queries.byte_burst);
    number("result_rate",s.results.rate);number("result_burst",s.results.burst);number("result_byte_rate",s.results.byte_rate);number("result_byte_burst",s.results.byte_burst);
    json.pop_back();json+="}}";return json;
}
std::string transfers_json(const transfers::Statistics& statistics, const transfers::SecuritySettings& settings) {
    std::string json{"{\"statistics\":{"};
    for(std::size_t i=0;i<transfers::statistic_names.size();++i){
        if(i){json+=',';}json+='\"';json+=transfers::statistic_names[i];json+="\":";json+=std::to_string(statistics.values[i]);
    }
    json+="},\"configuration\":{";
    const auto boolean=[&](std::string_view key,bool enabled){json+='\"';json+=key;json+=enabled?"\":true,":"\":false,";};
    const auto number=[&](std::string_view key,std::uint32_t count){json+='\"';json+=key;json+="\":";json+=std::to_string(count);json+=',';};
    boolean("ctm_enabled",settings.ctm_enabled);boolean("rcm_enabled",settings.rcm_enabled);
    boolean("guest_initiate",settings.guests.initiate);boolean("guest_receive",settings.guests.receive);
    number("request_rate",settings.flood.requests.rate);number("request_burst",settings.flood.requests.burst);
    number("request_byte_rate",settings.flood.requests.byte_rate);number("request_byte_burst",settings.flood.requests.byte_burst);
    number("incoming_rate",settings.flood.incoming.rate);number("incoming_burst",settings.flood.incoming.burst);
    number("incoming_byte_rate",settings.flood.incoming.byte_rate);number("incoming_byte_burst",settings.flood.incoming.byte_burst);
    json+="\"allowed_protocols\":";
    if (settings.allowed_protocols.any()) { json+="\"any\""; }
    else {
        json+='[';
        bool first=true;
        for (const auto& name : settings.allowed_protocols.names()) {
            if (!first) { json+=','; } first=false;json+='\"';
            for (const char c : name) { if(c=='\"'||c=='\\'){json+='\\';}json+=c; }
            json+='\"';
        }
        json+=']';
    }
    json+="}}";return json;
}
std::string monitoring_json(const Snapshot& value) {
    std::string json = std::string("{\"available\":") + (value.process_available ? "true" : "false");
    const auto field = [&](std::string_view key, std::uint64_t count) {
        json += ",\"";
        json += key;
        json += "\":\"";
        json += std::to_string(count);
        json += '"';
    };
    field("cpu_user_ms", value.cpu_user_ms);
    field("cpu_system_ms", value.cpu_system_ms);
    field("cpu_window_ms", value.cpu_window_ms);
    field("cpu_delta_ms", value.cpu_delta_ms);
    field("rss_bytes", value.rss_bytes);
    field("sessions_accepted", value.sessions_accepted);
    field("sessions_negotiating", value.sessions_negotiating);
    field("sessions_identified", value.sessions_identified);
    field("sessions_ready", value.sessions_ready);
    field("sessions_closing", value.sessions_closing);
    field("sessions_closed", value.sessions_closed);
    field("sessions_created", value.sessions_created);
    field("sessions_completed", value.sessions_completed);
    field("login_samples", value.login_samples);
    field("login_last_ms", value.login_last_ms);
    field("login_max_ms", value.login_max_ms);
    field("reactor_samples", value.reactor_samples);
    field("reactor_last_ms", value.reactor_last_ms);
    field("reactor_max_ms", value.reactor_max_ms);
    json += '}';
    return json;
}
std::string status_json(const Snapshot& value) {
    std::string chat_json{"{"};
    // Names are a fixed allowlist; values contain aggregate counters only.
    for (std::size_t i = 0; i < chat::statistic_names.size(); ++i) {
        if (i != 0) { chat_json += ','; }
        chat_json += '\"'; chat_json += chat::statistic_names[i]; chat_json += "\":";
        chat_json += std::to_string(value.chat.values[i]);
    }
    chat_json += '}';
    std::string kick_json="{";
    for(std::size_t i=0;i<admin::kick_statistic_names.size();++i){
        if(i){kick_json+=',';}kick_json+='"';kick_json+=admin::kick_statistic_names[i];kick_json+="\":\"";
        kick_json+=std::to_string(value.kick.values[i]);kick_json+='"';
    }
    kick_json+='}';
    std::string mute_json="{";
    for(std::size_t i=0;i<admin::mute_statistic_names.size();++i){
        if(i){mute_json+=',';}mute_json+='"';mute_json+=admin::mute_statistic_names[i];mute_json+="\":\"";
        mute_json+=std::to_string(value.mute.values[i]);mute_json+='"';
    }
    mute_json+='}';
    return "{\"api_version\":\"v1\",\"server\":{\"name\":\"dc24h.eu\",\"version\":\"" +
        std::string(dc24h::version) + "\",\"core_running\":" + (value.core_running ? "true" : "false") +
        ",\"uptime_seconds\":" + std::to_string(value.uptime_seconds) + "},\"network\":{\"active_connections\":" +
        std::to_string(value.active_connections) + ",\"accepted_connections\":" + std::to_string(value.accepted_connections) +
        ",\"adc_frames\":" + std::to_string(value.adc_frames) + ",\"adcs_handshakes\":" + std::to_string(value.adcs_handshakes) +
        ",\"security_rejected\":" + std::to_string(value.security_rejected) + ",\"security_closed\":" +
        std::to_string(value.security_closed) + ",\"rejected_connections\":" + std::to_string(value.rejected_connections) +
        ",\"closed_connections\":" + std::to_string(value.closed_connections) +
        ",\"received_bytes\":" + std::to_string(value.received_bytes) + ",\"sent_bytes\":" + std::to_string(value.sent_bytes) +
        ",\"peak_connections\":" + std::to_string(value.peak_connections) + "},\"users\":{\"online\":" +
        std::to_string(value.online_users) + ",\"peak_online\":" + std::to_string(value.peak_online_users) +
        ",\"successful_logins\":" + std::to_string(value.successful_logins) + "},\"chat\":" + chat_json + ",\"kick\":"+kick_json+",\"mute\":"+mute_json+",\"search\":" + search_json(value) + ",\"transfers\":" + transfers_json(value.transfers, value.transfer_settings) + ",\"database\":{\"enabled\":" +
        (value.database_enabled ? "true" : "false") + "},\"dashboard\":" + dashboard_json(value) +
        (value.configuration ? ",\"configuration\":" + configuration_json(*value.configuration) : "") +
        ",\"monitoring\":" + monitoring_json(value) + "}\n";
}
} // namespace

void validate(const Options& options) {
    const auto path = std::filesystem::path(options.socket_path);
    if (!path_text(options.socket_path) || !path_text(options.token_file) ||
        options.max_connections == 0 || options.max_connections > 128 ||
        options.request_timeout == 0 || options.request_timeout > 30 ||
        (!options.socket_path.empty() && (!path.is_absolute() || options.socket_path.size() >= sizeof(sockaddr_un::sun_path) ||
            path.filename().empty() || path.filename().string().size() > 64 || path.filename() == "." || path.filename() == "..")) ||
        (options.enabled && (options.socket_path.empty() || options.token_file.empty()))) {
        throw std::invalid_argument("Invalid local API settings");
    }
}

struct Server::Impl {
    logging::Logger& logger;
    Options options;
    StatusProvider status;
    FailureHandler failure;
    AccountHandler accounts;
    UsersProvider users;
    std::string token;
    Fd directory;
    Fd listener;
    std::string leaf;
    struct stat owned{};
    bool bound{false};
    bool started{false};
    std::atomic<bool> failed{false};
    std::jthread worker;

    Impl(logging::Logger& log, Options config, StatusProvider provider, FailureHandler handler, AccountHandler account_handler, UsersProvider user_provider)
        : logger(log), options(std::move(config)), status(std::move(provider)), failure(std::move(handler)), accounts(std::move(account_handler)), users(std::move(user_provider)) {
        validate(options);
        if (!options.enabled || !status || !failure) { throw std::invalid_argument("Invalid local API settings"); }
        try { token = security::read_private_secret(options.token_file); }
        catch (const std::exception&) { throw std::runtime_error("Cannot read private API token file"); }
        if (!valid_token(token)) {
            OPENSSL_cleanse(token.data(), token.size());
            throw std::runtime_error("API token must contain 64 hexadecimal characters");
        }
    }
    ~Impl() {
        worker.request_stop();
        if (worker.joinable()) { worker.join(); }
        cleanup();
        OPENSSL_cleanse(token.data(), token.size());
    }
    void cleanup() noexcept {
        listener.reset();
        if (bound) {
            struct stat current{};
            if (::fstatat(directory.value, leaf.c_str(), &current, AT_SYMLINK_NOFOLLOW) == 0 &&
                S_ISSOCK(current.st_mode) && current.st_dev == owned.st_dev && current.st_ino == owned.st_ino) {
                (void)::unlinkat(directory.value, leaf.c_str(), 0);
            }
            bound = false;
        }
        directory.reset();
    }
    void bind_socket() {
        const auto path = std::filesystem::path(options.socket_path);
        directory = Fd(::open(path.parent_path().c_str(), O_DIRECTORY | O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
        struct stat parent{};
        if (directory.value < 0 || ::fstat(directory.value, &parent) != 0 ||
            parent.st_uid != ::geteuid() || (parent.st_mode & 0077) != 0) {
            throw std::runtime_error("API socket requires a private owner-controlled directory");
        }
        leaf = path.filename().string();
        struct stat existing{};
        if (::fstatat(directory.value, leaf.c_str(), &existing, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT) {
            throw std::runtime_error("API socket path already exists or is inaccessible");
        }
        listener = Fd(::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        if (listener.value < 0) { throw std::runtime_error("Cannot create local API socket"); }
        // Anchor creation/cleanup to the checked private directory, without chdir/umask.
        const auto anchored = "/proc/self/fd/" + std::to_string(directory.value) + "/" + leaf;
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (anchored.size() >= sizeof(address.sun_path)) { throw std::runtime_error("Invalid local API settings"); }
        std::memcpy(address.sun_path, anchored.c_str(), anchored.size() + 1);
        if (::bind(listener.value, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            throw std::runtime_error("Cannot bind local API socket");
        }
        if (::fstatat(directory.value, leaf.c_str(), &owned, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISSOCK(owned.st_mode)) {
            throw std::runtime_error("Cannot inspect local API socket");
        }
        bound = true;
        if (::fchmodat(directory.value, leaf.c_str(), 0600, 0) != 0 ||
            ::listen(listener.value, static_cast<int>(options.max_connections)) != 0) {
            throw std::runtime_error("Cannot listen on local API socket");
        }
    }
    std::string dispatch(const Request& request) {
        const bool head = request.method == "HEAD";
        if (!authenticate(request.authorization, token)) { return response(401, "{\"error\":\"unauthorized\"}\n", head); }
        if (accounts && (request.target.starts_with("/api/v1/accounts") || request.target == "/api/v1/admin" || request.target == "/api/v1/admin/access" || request.target.starts_with("/api/v1/bans") || request.target == "/api/v1/audit" || request.target == "/api/v1/management")) { return accounts(request); }
        if (request.method != "GET") { return response(405, "{\"error\":\"method_not_allowed\"}\n", head); }
        if (request.target == "/api/v1/health") { return response(200, "{\"status\":\"ok\"}\n"); }
        if (request.target == "/api/v1/status") { return response(200, status_json(status())); }
        if (request.target == "/api/v1/users" && users) { return response(200, users_json(users())); }
        return response(404, "{\"error\":\"not_found\"}\n");
    }
    bool service(Peer& peer, short events) {
        if (Clock::now() - peer.accepted >= std::chrono::seconds(options.request_timeout) ||
            (events & (POLLERR | POLLNVAL)) != 0) { return false; }
        if (peer.output.empty() && (events & (POLLIN | POLLHUP)) != 0) {
            std::array<char, 4096> buffer{};
            struct Wipe { std::array<char,4096>& bytes; ~Wipe(){OPENSSL_cleanse(bytes.data(),bytes.size());} } wipe{buffer};
            const auto count = ::recv(peer.fd.value, buffer.data(),
                std::min(buffer.size(), max_request_bytes + 1 - peer.input.size()), 0);
            if (count == 0) { return false; }
            if (count < 0) { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
            peer.input.append(buffer.data(), static_cast<std::size_t>(count));
            OPENSSL_cleanse(buffer.data(), buffer.size());
            auto parsed = parse_request(peer.input, bool(accounts));
            struct Erase {
                Request& request;
                ~Erase() {
                    OPENSSL_cleanse(request.body.data(), request.body.size());
                    OPENSSL_cleanse(request.admin_grant.data(), request.admin_grant.size());
                    OPENSSL_cleanse(request.admin_csrf.data(), request.admin_csrf.size());
                    OPENSSL_cleanse(request.admin_session.data(), request.admin_session.size());
                }
            } erase{parsed.request};
            if (parsed.state == ParseState::complete) {
                peer.output = dispatch(parsed.request);
                OPENSSL_cleanse(peer.input.data(), peer.input.size()); peer.input.clear();
            }
            if (parsed.state == ParseState::invalid) { peer.output = response(400, "{\"error\":\"bad_request\"}\n"); }
            if (parsed.state == ParseState::too_large) { peer.output = response(431, "{\"error\":\"headers_too_large\"}\n"); }
        }
        if (!peer.output.empty() && (events & (POLLOUT | POLLHUP)) != 0) {
            const auto count = ::send(peer.fd.value, peer.output.data() + peer.sent, peer.output.size() - peer.sent, MSG_NOSIGNAL);
            if (count < 0) { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
            peer.sent += static_cast<std::size_t>(count);
            if (peer.sent == peer.output.size()) { return false; }
        }
        return true;
    }
    void run(std::stop_token stop) {
        std::vector<Peer> peers;
        peers.reserve(options.max_connections);
        std::vector<pollfd> watches;
        watches.reserve(options.max_connections + 1);
        while (!stop.stop_requested()) {
            watches.clear();
            watches.push_back({listener.value, POLLIN, 0});
            for (const auto& peer : peers) { watches.push_back({peer.fd.value, static_cast<short>(peer.output.empty() ? POLLIN : POLLOUT), 0}); }
            const int ready = ::poll(watches.data(), watches.size(), 100);
            if (ready < 0 && errno == EINTR) { continue; }
            if (ready < 0 || (watches[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                throw std::runtime_error("Local API worker failed");
            }
            if (stop.stop_requested()) { break; }
            for (std::size_t i = peers.size(); i-- > 0;) {
                if (!service(peers[i], watches[i + 1].revents)) { peers.erase(peers.begin() + static_cast<std::ptrdiff_t>(i)); }
            }
            if ((watches[0].revents & POLLIN) == 0) { continue; }
            for (unsigned int attempts = 0; attempts < 16 && !stop.stop_requested(); ++attempts) {
                Fd socket(::accept4(listener.value, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC));
                if (socket.value < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) { break; }
                    if (errno == EINTR || errno == ECONNABORTED) { continue; }
                    throw std::runtime_error("Local API worker failed");
                }
                ucred credentials{};
                socklen_t size = sizeof(credentials);
                if (peers.size() >= options.max_connections ||
                    ::getsockopt(socket.value, SOL_SOCKET, SO_PEERCRED, &credentials, &size) != 0 ||
                    size != sizeof(credentials) || credentials.uid != ::geteuid()) { continue; }
                peers.emplace_back(std::move(socket));
            }
        }
    }
};

Server::Server(logging::Logger& logger, Options options, StatusProvider status, FailureHandler failure, AccountHandler accounts, UsersProvider users)
    : impl_(std::make_unique<Impl>(logger, std::move(options), std::move(status), std::move(failure), std::move(accounts), std::move(users))) {}
Server::~Server() = default;
void Server::start() {
    if (impl_->started) { throw std::logic_error("Local API start may only be called once"); }
    impl_->started = true;
    try {
        impl_->bind_socket();
        impl_->worker = std::jthread([this](std::stop_token stop) {
            try { impl_->run(stop); }
            catch (...) {
                impl_->failed.store(true);
                impl_->logger.error("Local API worker failed");
                try { impl_->failure(); } catch (...) {}
            }
        });
        impl_->logger.info("Local API listening");
    } catch (...) { impl_->cleanup(); throw; }
}
bool Server::failed() const noexcept { return impl_->failed.load(); }
} // namespace dc24h::api
