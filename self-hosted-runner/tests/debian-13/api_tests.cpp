#include "dc24h/api/http.hpp"
#include "dc24h/api/server.hpp"
#include "dc24h/logging/logger.hpp"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace {
using namespace dc24h::api;
void require(bool condition, std::source_location at = std::source_location::current()) {
    if (!condition) { throw std::runtime_error("API assertion failed at line " + std::to_string(at.line())); }
}
template<typename Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected);
}
void parser() {
    const std::string token(64, 'a');
    require(valid_token(token));
    require(!valid_token(std::string(63, 'a')));
    require(!valid_token(std::string(64, 'g')));
    require(authenticate("bEaReR " + token, token));
    require(!authenticate("Bearer " + std::string(64, 'b'), token));
    require(!authenticate("Bearer  " + token, token));
    require(!authenticate("Basic " + token, token));
    const std::string request = "GET /api/v1/status HTTP/1.1\r\nHost: local\r\nAuthorization: Bearer " + token + "\r\n\r\n";
    for (std::size_t i = 0; i < request.size(); ++i) {
        require(parse_request(std::string_view(request).substr(0, i)).state == ParseState::incomplete);
    }
    const auto good = parse_request(request);
    const std::string form_header="POST /api/v1/accounts HTTP/1.1\r\nHost: local\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 7\r\n\r\n";
    const auto form_request=form_header+"a=b%20c";
    for(std::size_t i=0;i<form_request.size();++i){require(parse_request(std::string_view(form_request).substr(0,i),true).state==ParseState::incomplete);}
    require(parse_request(form_request,true).request.body=="a=b%20c");
    require(parse_request(form_request).state==ParseState::invalid);
    require(parse_request(form_request+"x",true).state==ParseState::invalid);
    require(parse_request("GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\n\r\nx",true).state==ParseState::invalid);
    require(parse_request("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 01\r\n\r\nx",true).state==ParseState::invalid);
    require(good.state == ParseState::complete && good.request.target == "/api/v1/status");
    require(good.request.method == "GET" && authenticate(good.request.authorization, token));
    require(parse_request(request + "x").state == ParseState::invalid);
    for (const std::string value : {
        "GET / HTTP/1.0\r\nHost: x\r\n\r\n", "GET / HTTP/1.1\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: \r\n\r\n", "GET / HTTP/1.1\r\nHost: x,y\r\n\r\n",
        "GET //x HTTP/1.1\r\nHost: x\r\n\r\n", "GET http://x/ HTTP/1.1\r\nHost: x\r\n\r\n",
        "GET / HTTP/1.1\nHost: x\n\n", "GET / HTTP/1.1\rX", "GET / HTTP/1.1\r\nHost : x\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: x\r\nHOST: x\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 00\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: x\r\nExpect: 100-continue\r\n\r\n",
        "GET / HTTP/1.1\r\nHost: x\r\n folded\r\n\r\n",
    }) { require(parse_request(value).state == ParseState::invalid); }
    const std::string prefix = "GET / HTTP/1.1\r\nHost: x\r\nX-Pad: ";
    const auto boundary = prefix + std::string(max_request_bytes - prefix.size() - 4, 'a') + "\r\n\r\n";
    require(parse_request(boundary).state == ParseState::complete);
    require(parse_request(boundary + "x").state == ParseState::too_large);
    std::string headers = "GET / HTTP/1.1\r\nHost: x\r\n";
    for (unsigned int i = 0; i < 31; ++i) { headers += "X-" + std::to_string(i) + ": x\r\n"; }
    require(parse_request(headers + "\r\n").state == ParseState::complete);
    require(parse_request(headers + "Too-Many: x\r\n\r\n").state == ParseState::invalid);
    const auto grant = std::string(64, 'A');
    const auto granted = parse_request("GET / HTTP/1.1\r\nHost: x\r\nX-DC24H-Admin-Grant: " + grant + "\r\n\r\n");
    require(granted.state == ParseState::complete && granted.request.admin_grant == grant);
    require(parse_request("GET / HTTP/1.1\r\nHost: x\r\nX-DC24H-Admin-Grant: ab\r\n\r\n").state == ParseState::invalid);
    require(parse_request("GET / HTTP/1.1\r\nHost: x\r\nX-DC24H-Admin-Grant: " + grant + "\r\nX-DC24H-Admin-Grant: " + grant + "\r\n\r\n").state == ParseState::invalid);
    const auto session = std::string(32, 'a');
    const auto csrf = std::string(64, 'b');
    const auto bound = parse_request("POST / HTTP/1.1\r\nHost: x\r\nX-DC24H-Session: " + session + "\r\nX-DC24H-Csrf: " + csrf + "\r\n\r\n");
    require(bound.state == ParseState::complete && bound.request.admin_session == session && bound.request.admin_csrf == csrf);
    require(parse_request("GET / HTTP/1.1\r\nHost: x\r\nX-DC24H-Session: " + std::string(32, 'A') + "\r\n\r\n").state == ParseState::invalid);
    require(parse_request("GET / HTTP/1.1\r\nHost: x\r\nX-DC24H-Csrf: ab\r\n\r\n").state == ParseState::invalid);
    require(parse_request("GET / HTTP/1.1\r\nHost: x\r\nX-DC24H-Session: " + session + "\r\nX-DC24H-Session: " + session + "\r\n\r\n").state == ParseState::invalid);
    const auto head = response(405, "{}", true);
    require(head.ends_with("\r\n\r\n") && head.find("Allow: GET") != std::string::npos);
    require(response(401, "{}").find("WWW-Authenticate: Bearer") != std::string::npos);
    std::mt19937 random(0x019);
    for (unsigned int iteration = 0; iteration < 5000; ++iteration) {
        std::string value(random() % 9000, ' ');
        for (auto& c : value) { c = static_cast<char>(random() & 255); }
        (void)parse_request(value);
    }
}
void dashboard_contract() {
    const auto zero = dashboard_json(Snapshot{});
    require(zero ==
        "{\"schema\":1,\"core_running\":false,\"database_enabled\":false,\"uptime_seconds\":\"0\","
        "\"users_online\":\"0\",\"users_peak\":\"0\",\"users_logins\":\"0\","
        "\"connections_active\":\"0\",\"connections_peak\":\"0\",\"connections_accepted\":\"0\","
        "\"connections_rejected\":\"0\",\"connections_closed\":\"0\",\"security_rejected\":\"0\","
        "\"security_closed\":\"0\",\"adc_frames\":\"0\",\"adcs_handshakes\":\"0\","
        "\"bytes_received\":\"0\",\"bytes_sent\":\"0\",\"chat_received\":\"0\",\"chat_public\":\"0\","
        "\"chat_private\":\"0\",\"chat_rejected\":\"0\",\"chat_flood_limited\":\"0\","
        "\"search_queries\":\"0\",\"search_routed\":\"0\",\"search_rejected\":\"0\","
        "\"search_results\":\"0\",\"search_tracked\":\"0\",\"transfers_ctm\":\"0\",\"transfers_rcm\":\"0\","
        "\"transfers_peer_unsupported\":\"0\",\"transfers_peer_failed\":\"0\","
        "\"kicks_succeeded\":\"0\",\"kicks_refused\":\"0\",\"mutes_applied\":\"0\","
        "\"mutes_public_blocked\":\"0\",\"mutes_private_blocked\":\"0\"}");
    require(zero.size() < 900);
    Snapshot value{};
    value.core_running = true;
    value.database_enabled = true;
    value.uptime_seconds = 3661;
    value.online_users = 4;
    value.peak_online_users = 6;
    value.successful_logins = 23;
    value.active_connections = 7;
    value.peak_connections = 9;
    value.accepted_connections = 48;
    value.rejected_connections = 5;
    value.closed_connections = 41;
    value.security_rejected = 2;
    value.security_closed = 1;
    value.adc_frames = 144;
    value.adcs_handshakes = 3;
    value.received_bytes = std::numeric_limits<std::uint64_t>::max();
    value.sent_bytes = 8192;
    value.chat.values[0] = 12;
    value.chat.values[1] = 5;
    value.chat.values[2] = 3;
    value.chat.values[3] = 2;
    value.chat.values[4] = 1;
    value.search.values[0] = 1;
    value.search.values[1] = 2;
    value.search.values[2] = 3;
    value.search.values[10] = 11;
    value.search.tracked_searches = 4;
    value.transfers.values[0] = 8;
    value.transfers.values[10] = 9;
    value.transfers.values[20] = 10;
    value.transfers.values[21] = 11;
    value.kick.values[1] = 12;
    value.kick.values[2] = 13;
    value.mute.values[1] = 14;
    value.mute.values[6] = 15;
    value.mute.values[7] = 16;
    const auto json = dashboard_json(value);
    require(json.find("\"core_running\":true") != std::string::npos);
    require(json.find("\"database_enabled\":true") != std::string::npos);
    require(json.find("\"uptime_seconds\":\"3661\"") != std::string::npos);
    require(json.find("\"users_logins\":\"23\"") != std::string::npos);
    require(json.find("\"bytes_received\":\"18446744073709551615\"") != std::string::npos);
    require(json.find("\"bytes_sent\":\"8192\"") != std::string::npos);
    require(json.find("\"chat_received\":\"12\"") != std::string::npos);
    require(json.find("\"chat_public\":\"5\"") != std::string::npos);
    require(json.find("\"chat_flood_limited\":\"1\"") != std::string::npos);
    require(json.find("\"search_queries\":\"1\"") != std::string::npos);
    require(json.find("\"search_routed\":\"2\"") != std::string::npos);
    require(json.find("\"search_rejected\":\"3\"") != std::string::npos);
    require(json.find("\"search_results\":\"11\"") != std::string::npos);
    require(json.find("\"search_tracked\":\"4\"") != std::string::npos);
    require(json.find("\"transfers_ctm\":\"8\"") != std::string::npos);
    require(json.find("\"transfers_rcm\":\"9\"") != std::string::npos);
    require(json.find("\"transfers_peer_unsupported\":\"10\"") != std::string::npos);
    require(json.find("\"transfers_peer_failed\":\"11\"") != std::string::npos);
    require(json.find("\"kicks_succeeded\":\"12\"") != std::string::npos);
    require(json.find("\"kicks_refused\":\"13\"") != std::string::npos);
    require(json.find("\"mutes_applied\":\"14\"") != std::string::npos);
    require(json.find("\"mutes_public_blocked\":\"15\"") != std::string::npos);
    require(json.find("\"mutes_private_blocked\":\"16\"") != std::string::npos);
    require(json.find("password") == std::string::npos);
    require(json.find("nickname") == std::string::npos);
}
void lifecycle() {
    char name[] = "/tmp/dc24h-api-unit-XXXXXX";
    require(::mkdtemp(name) != nullptr);
    const auto dir = std::filesystem::path(name);
    struct Cleanup { std::filesystem::path dir; ~Cleanup() { std::filesystem::remove_all(dir); } } cleanup{dir};
    const auto token = dir / "admin.token";
    { std::ofstream stream(token); stream << std::string(64, 'a') << '\n'; }
    require(::chmod(token.c_str(), 0600) == 0);
    Options options{true, (dir / "admin.sock").string(), token.string(), 16, 1};
    validate(options);
    for (const auto n : {0U, 129U}) { auto bad = options; bad.max_connections = n; rejects([&] { validate(bad); }); }
    for (const auto n : {0U, 31U}) { auto bad = options; bad.request_timeout = n; rejects([&] { validate(bad); }); }
    { auto bad = options; bad.socket_path = "relative.sock"; rejects([&] { validate(bad); }); }
    dc24h::logging::Logger logger;
    const auto status = [] { return Snapshot{}; };
    {
        Server never_started(logger, options, status, [] {});
        require(!std::filesystem::exists(options.socket_path));
    }
    {
        Server server(logger, options, status, [] {});
        server.start();
        struct stat state{};
        require(::lstat(options.socket_path.c_str(), &state) == 0 && S_ISSOCK(state.st_mode));
        require((state.st_mode & 0777) == 0600);
        rejects([&] { server.start(); });
        // A responsive API worker must not pretend an unstarted Core/Network
        // is ready. The historical /health endpoint remains a liveness probe.
        const int probe = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        require(probe >= 0);
        sockaddr_un address{}; address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, options.socket_path.c_str(), options.socket_path.size() + 1);
        require(::connect(probe, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        const auto request = "GET /api/v1/ready HTTP/1.1\r\nHost: x\r\nAuthorization: Bearer " + std::string(64, 'a') + "\r\n\r\n";
        require(::send(probe, request.data(), request.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(request.size()));
        std::string denial;
        std::array<char, 512> reply_bytes{};
        for (ssize_t received; (received = ::recv(probe, reply_bytes.data(), reply_bytes.size(), 0)) > 0;) {
            denial.append(reply_bytes.data(), static_cast<std::size_t>(received));
        }
        ::close(probe);
        require(denial.find("503 Service Unavailable") != std::string::npos &&
                denial.find("{\"error\":\"not_ready\"}") != std::string::npos);
        Server rival(logger, options, status, [] {});
        rejects([&] { rival.start(); });
        require(std::filesystem::exists(options.socket_path));
    }
    require(!std::filesystem::exists(options.socket_path));
    // Shutdown must preserve a replacement at the same pathname.
    {
        Server server(logger, options, status, [] {}); server.start();
        std::filesystem::rename(options.socket_path, dir / "moved.sock");
        { std::ofstream replacement(options.socket_path); replacement << "KEEP"; }
    }
    require(std::filesystem::is_regular_file(options.socket_path));
    std::filesystem::remove(options.socket_path);
    // Provider failures are contained by the worker, logged without exception data,
    // and trigger the owner's stop callback rather than std::terminate.
    std::atomic<bool> stopped{false};
    {
        Server server(logger, options, []() -> Snapshot { throw std::runtime_error("PRIVATE_CALLBACK"); },
                      [&] { stopped.store(true); });
        server.start();
        const int peer = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        require(peer >= 0);
        struct Close { int fd; ~Close() { ::close(fd); } } close{peer};
        sockaddr_un address{}; address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, options.socket_path.c_str(), options.socket_path.size() + 1);
        require(::connect(peer, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        const auto request = "GET /api/v1/status HTTP/1.1\r\nHost: x\r\nAuthorization: Bearer " + std::string(64, 'a') + "\r\n\r\n";
        require(::send(peer, request.data(), request.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(request.size()));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!stopped.load() && std::chrono::steady_clock::now() < deadline) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
        require(server.failed() && stopped.load());
    }
    require(!std::filesystem::exists(options.socket_path));
}
void users_contract() {
    dc24h::users::PresenceDirectory empty;
    require(users_json(empty) == "{\"schema\":1,\"truncated\":false,\"users\":[]}\n");
    dc24h::users::PresenceDirectory filled;
    filled.count = 1;
    auto& user = filled.users[0];
    user.session_id = std::numeric_limits<std::uint64_t>::max();
    std::memcpy(user.sid.data(), "ABCD", 4);
    const std::string nickname = "a\"b\\c";
    std::memcpy(user.nickname.data(), nickname.data(), nickname.size());
    user.nickname_len = static_cast<std::uint8_t>(nickname.size());
    user.transport = dc24h::users::Transport::adcs;
    user.role = dc24h::accounts::Role::owner;
    user.public_muted = true;
    const auto body = users_json(filled);
    require(body.find("\"session\":\"18446744073709551615\"") != std::string::npos);
    require(body.find("\"sid\":\"ABCD\"") != std::string::npos);
    require(body.find("\"nickname\":\"a\\\"b\\\\c\"") != std::string::npos);
    require(body.find("\"transport\":\"adcs\"") != std::string::npos);
    require(body.find("\"role\":\"owner\"") != std::string::npos);
    require(body.find("\"public_muted\":true") != std::string::npos);
    require(body.find("\"private_muted\":false") != std::string::npos);
    require(body.find("cid") == std::string::npos);
    require(body.find("password") == std::string::npos);
    require(body.find("email") == std::string::npos);
    user.nickname[0] = '\n';
    user.nickname_len = 1;
    const auto rejected = users_json(filled);
    require(rejected.find("\"truncated\":true") != std::string::npos);
    require(rejected.find("nickname") == std::string::npos);
    user.nickname[0] = 'a';
    user.nickname_len = 1;
    std::memcpy(user.sid.data(), "A00D", 4);
    const auto bad_sid = users_json(filled);
    require(bad_sid.find("\"truncated\":true") != std::string::npos);
    require(bad_sid.find("nickname") == std::string::npos);
}
}
int main() {
    try { parser(); dashboard_contract(); users_contract(); lifecycle(); std::cout << "API parser, authentication, dashboard, users and lifecycle OK\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
