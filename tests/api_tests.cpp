#include "dc24h/api/http.hpp"
#include "dc24h/api/server.hpp"
#include "dc24h/logging/logger.hpp"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
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
}
int main() {
    try { parser(); lifecycle(); std::cout << "API parser, authentication and lifecycle OK\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
