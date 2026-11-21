#include "dc24h/admin/crypto.hpp"
#include "dc24h/admin/server.hpp"
#include "dc24h/db/store.hpp"
#include "dc24h/net/server.hpp"

#include "../adc/test_messages.hpp"
#include "../db/test_database.hpp"
#include "../net/test_certificate.hpp"
#include "../net/test_client.hpp"

#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <future>
#include <thread>

namespace dc24h::admin {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using asio::ip::tcp;
using Json = nlohmann::json;
using Response = http::response<http::string_body>;
using Method = http::verb;
using Status = http::status;
using db::test::credentials;
using db::test::fixture_path;
using db::test::read_fixture;
using db::test::TestSql;
using net::test::RawClient;
using namespace std::chrono_literals;

constexpr std::string_view origin = "https://hub.example.test";
constexpr auto login_url = "/api/v1/auth/login";
constexpr auto logout_url = "/api/v1/auth/logout";
constexpr auto me_url = "/api/v1/auth/me";
constexpr auto status_url = "/api/v1/status";
constexpr auto users_url = "/api/v1/users";
constexpr auto bans_url = "/api/v1/bans";
constexpr auto audit_url = "/api/v1/audit";
constexpr auto ban_json = R"({"kind":"ip","identifier":"192.0.2.10","reason":"Fixture only"})";

net::ServerConfig hub_config(const net::test::TestCertificate &certificate) {
    net::ServerConfig config;
    config.adc = net::Endpoint{"127.0.0.1", 0};
    config.adcs.port = 0;
    config.certificate = certificate.cert_path;
    config.private_key = certificate.key_path;
    config.limits.shutdown_timeout = 100ms;
    return config;
}

class RunningHub {
  public:
    RunningHub()
        : server(io_, hub_config(certificate_), {}, credentials("reader")),
          worker_([this] { io_.run(); }) {}

    ~RunningHub() {
        server.stop();
        worker_.join();
    }

  private:
    net::test::TestCertificate certificate_;
    asio::io_context io_;

  public:
    net::Server server;

  private:
    std::jthread worker_;
};

struct Session {
    std::string cookie;
    std::string csrf;
};

class AdminApi : public testing::Test {
  protected:
    void SetUp() override {
        TestSql sql;
        sql.execute("DELETE FROM admin_sessions");
        sql.execute("TRUNCATE TABLE bans");
        sql.execute("TRUNCATE TABLE audit_events");
        std::filesystem::remove(socket_);
        std::filesystem::create_symlink(fixture_path("mdb.sock"), socket_);
        auto config = credentials("api");
        config.socket = socket_;
        api_ = std::make_unique<Server>(hub_.server, std::move(config),
                                        Config{0, std::string(origin)});
    }

    Response call(Method method, std::string_view path, const Session &session = {},
                  std::string_view payload = {}, std::string_view source = origin,
                  const char *media_type = "application/json") {
        http::request<http::string_body> request(method, to_beast(path), 11);
        request.set(http::field::host, "localhost");
        if (!source.empty()) {
            request.set("Origin", to_beast(source));
        }
        if (!session.cookie.empty()) {
            request.set(http::field::cookie, session.cookie);
        }
        if (!session.csrf.empty()) {
            request.set("X-CSRF-Token", session.csrf);
        }
        if (!payload.empty()) {
            request.set(http::field::content_type, media_type);
            request.body() = payload;
        }
        request.prepare_payload();
        asio::io_context io;
        tcp::socket socket(io);
        socket.connect({asio::ip::address_v4::loopback(), api_->port()});
        http::write(socket, request);
        return read(socket);
    }

    Response raw_request(std::string_view wire) {
        asio::io_context io;
        tcp::socket socket(io);
        socket.connect({asio::ip::address_v4::loopback(), api_->port()});
        asio::write(socket, asio::buffer(wire));
        return read(socket);
    }

    Response login(std::string_view username, std::string_view password,
                   std::string_view source = origin) {
        return call(Method::post, login_url, {},
                    Json{{"username", username}, {"password", password}}.dump(), source);
    }

    Session sign_in(std::string_view role = "admin") {
        const auto name = std::string(role);
        const auto response = login(name + "-user", read_fixture("login-" + name + ".pw"));
        EXPECT_EQ(response.result(), Status::ok);
        const auto cookie = response[http::field::set_cookie];
        EXPECT_FALSE(cookie.empty());
        return {std::string(cookie.substr(0, cookie.find(';'))),
                Json::parse(response.body()).at("csrf_token").get<std::string>()};
    }

    static std::string code(const Response &response) {
        return Json::parse(response.body()).at("error").at("code").get<std::string>();
    }

    net::Endpoint adc() const { return hub_.server.adc_endpoint().value(); }

    static std::string enter(RawClient &client, std::string_view nickname,
                             std::string_view cid = adc::test::cid,
                             std::string_view pid = adc::test::pid) {
        client.send("HSUP ADBASE ADTIGR\n");
        EXPECT_EQ(client.line(), "ISUP ADTIGR ADBASE\n");
        const auto sid_frame = adc::test::decode(client.line());
        const auto sid = std::string(adc::parse_sid(sid_frame).value());
        EXPECT_TRUE(client.line().starts_with("IINF "));
        client.send("BINF " + sid + " ID" + std::string(cid) + " PD" + std::string(pid) + " NI" +
                    std::string(nickname) + '\n');
        return sid;
    }

    const std::filesystem::path socket_ = fixture_path("api-alternate.sock");

  private:
    static beast::string_view to_beast(std::string_view value) {
        return {value.data(), value.size()};
    }

    static Response read(tcp::socket &socket) {
        beast::flat_buffer buffer;
        Response response;
        http::read(socket, buffer, response);
        return response;
    }

    RunningHub hub_;
    std::unique_ptr<Server> api_;
};

TEST_F(AdminApi, RejectsUnauthorizedAccessAndBoundsHttpInputs) {
    for (const auto *path : {me_url, status_url, users_url, bans_url, audit_url}) {
        const auto response = call(Method::get, path);
        EXPECT_EQ(response.result(), Status::unauthorized);
        EXPECT_EQ(code(response), "unauthorized");
        EXPECT_EQ(response[http::field::cache_control], "no-store");
    }
    EXPECT_EQ(call(Method::post, logout_url).result(), Status::unauthorized);
    EXPECT_EQ(call(Method::post, bans_url, {}, ban_json).result(), Status::unauthorized);
    EXPECT_EQ(call(Method::delete_, "/api/v1/bans/1").result(), Status::unauthorized);
    EXPECT_EQ(call(Method::get, "/api/v2/status").result(), Status::not_found);
    const auto wrong_method = call(Method::patch, bans_url);
    EXPECT_EQ(wrong_method.result(), Status::method_not_allowed);
    EXPECT_EQ(wrong_method[http::field::allow], "GET, POST");
    const auto over = raw_request("POST /api/v1/auth/login HTTP/1.1\r\nHost: localhost\r\n"
                                  "Content-Length: 8193\r\n\r\n");
    EXPECT_EQ(over.result(), Status::payload_too_large);
    EXPECT_EQ(code(over), "payload_too_large");
    const auto headers =
        raw_request("GET /api/v1/status HTTP/1.1\r\nX-Pad: " + std::string(8193, 'a') + "\r\n\r\n");
    EXPECT_EQ(headers.result(), Status::request_header_fields_too_large);
}

TEST_F(AdminApi, ValidatesJsonAndReturnsGenericLoginFailures) {
    for (const auto body : {"{", "[]", R"({"username":"admin-user","password":7})",
                            R"({"username":"admin-user","username":"other","password":"value"})",
                            R"({"username":"admin-user","password":{"nested":"value"}})"}) {
        const auto response = call(Method::post, login_url, {}, body);
        EXPECT_EQ(response.result(), Status::bad_request);
        EXPECT_EQ(code(response), "invalid_json");
    }
    std::string invalid = R"({"username":"admin-user","password":"value"})";
    invalid.push_back('\0');
    EXPECT_EQ(call(Method::post, login_url, {}, invalid).result(), Status::bad_request);
    EXPECT_EQ(
        call(Method::post, login_url, {}, "username=x", origin, "application/x-www-form-urlencoded")
            .result(),
        Status::unsupported_media_type);
    const auto bad_password = login("admin-user", "wrong");
    const auto unknown_user = login("unknown-user", "wrong");
    EXPECT_EQ(bad_password.result(), Status::unauthorized);
    EXPECT_EQ(unknown_user.result(), Status::unauthorized);
    EXPECT_EQ(bad_password.body(), unknown_user.body());
    EXPECT_TRUE(bad_password[http::field::set_cookie].empty());
}

TEST_F(AdminApi, IssuesProtectedCookiesAndRevokesTheSessionOnLogout) {
    const auto response = login("admin-user", read_fixture("login-admin.pw"));
    ASSERT_EQ(response.result(), Status::ok);
    const auto cookie = std::string(response[http::field::set_cookie]);
    EXPECT_TRUE(cookie.starts_with("__Host-dc24h_session="));
    EXPECT_TRUE(cookie.ends_with("; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=28800"));
    const auto body = Json::parse(response.body());
    EXPECT_EQ(body.at("role"), "admin");
    EXPECT_EQ(body.at("username"), "admin-user");
    const Session session{cookie.substr(0, cookie.find(';')), body.at("csrf_token")};
    const auto token = std::string_view(session.cookie).substr(session.cookie.find('=') + 1);
    db::Store store(credentials("api"));
    ASSERT_TRUE(store.admin_session(session_digest(token)));
    EXPECT_FALSE(store.admin_session(token));

    const auto me = call(Method::get, me_url, session);
    EXPECT_EQ(me.result(), Status::ok);
    EXPECT_EQ(Json::parse(me.body()), body);
    const auto ambiguous = Session{session.cookie + "; " + session.cookie, session.csrf};
    EXPECT_EQ(call(Method::get, me_url, ambiguous).result(), Status::unauthorized);
    const auto logout = call(Method::post, logout_url, session);
    EXPECT_EQ(logout.result(), Status::no_content);
    EXPECT_TRUE(logout.body().empty());
    EXPECT_TRUE(std::string(logout[http::field::set_cookie]).ends_with("Max-Age=0"));
    EXPECT_EQ(call(Method::get, me_url, session).result(), Status::unauthorized);
    EXPECT_FALSE(store.admin_session(session_digest(token)));
}

TEST_F(AdminApi, ExpiresSessionsAndSupportsOfflineRevocation) {
    const auto expired = sign_in();
    TestSql sql;
    sql.execute("UPDATE admin_sessions SET expires_at = UTC_TIMESTAMP(6)");
    EXPECT_EQ(call(Method::get, me_url, expired).result(), Status::unauthorized);
    const auto revoked = sign_in();
    const auto other = sign_in("viewer");
    db::Store provisioner(credentials("provisioner"));
    EXPECT_EQ(provisioner.revoke_admin_sessions("admin-user"), 1U);
    EXPECT_EQ(call(Method::get, me_url, revoked).result(), Status::unauthorized);
    EXPECT_EQ(call(Method::get, me_url, other).result(), Status::ok);
}

TEST_F(AdminApi, IssuesIndependentSessionsOnConcurrentLogins) {
    auto first = std::async(std::launch::async, [&] { return sign_in(); });
    const auto second = sign_in();
    const auto session = first.get();
    EXPECT_NE(session.cookie, second.cookie);
    EXPECT_EQ(call(Method::post, logout_url, session).result(), Status::no_content);
    EXPECT_EQ(call(Method::get, me_url, second).result(), Status::ok);
}

TEST_F(AdminApi, LimitsRepeatedLoginAttempts) {
    for (unsigned attempt = 0; attempt < 5; ++attempt) {
        EXPECT_EQ(login("admin-user", "wrong").result(), Status::unauthorized);
    }
    const auto blocked = login("admin-user", read_fixture("login-admin.pw"));
    EXPECT_EQ(blocked.result(), Status::too_many_requests);
    EXPECT_EQ(blocked[http::field::retry_after], "60");
    EXPECT_EQ(login("viewer-user", read_fixture("login-viewer.pw")).result(), Status::ok);

    // New names also compete for the process-wide limit, even during an outage.
    std::filesystem::remove(socket_);
    for (unsigned attempt = 0; attempt < 24; ++attempt) {
        EXPECT_EQ(login("unregistered-" + std::to_string(attempt), "test").result(),
                  Status::service_unavailable);
    }
    EXPECT_EQ(login("another-user", "test").result(), Status::too_many_requests);
}

TEST_F(AdminApi, ChecksOriginAndSessionBoundCsrfForMutations) {
    const auto password = read_fixture("login-admin.pw");
    EXPECT_EQ(login("admin-user", password, "https://other.example.test").result(),
              Status::forbidden);
    EXPECT_EQ(login("admin-user", password, "").result(), Status::forbidden);
    const auto first = sign_in();
    const auto second = sign_in();
    const auto no_token = Session{first.cookie, {}};
    const auto wrong_token = Session{first.cookie, second.csrf};
    EXPECT_EQ(code(call(Method::post, bans_url, no_token, ban_json)), "invalid_csrf");
    EXPECT_EQ(call(Method::post, logout_url, wrong_token).result(), Status::forbidden);
    EXPECT_EQ(code(call(Method::post, bans_url, first, ban_json, "https://other.example.test")),
              "invalid_origin");
    EXPECT_EQ(call(Method::delete_, "/api/v1/bans/1", no_token).result(), Status::forbidden);
    EXPECT_EQ(call(Method::get, me_url, first).result(), Status::ok);
}

TEST_F(AdminApi, AllowsViewerReadsAndRejectsViewerWrites) {
    const auto viewer = sign_in("viewer");
    for (const auto *path : {me_url, status_url, users_url, bans_url, audit_url}) {
        EXPECT_EQ(call(Method::get, path, viewer).result(), Status::ok);
    }
    const auto add = call(Method::post, bans_url, viewer, ban_json);
    EXPECT_EQ(add.result(), Status::forbidden);
    EXPECT_EQ(code(add), "forbidden");
    EXPECT_EQ(call(Method::delete_, "/api/v1/bans/1", viewer).result(), Status::forbidden);
    EXPECT_EQ(call(Method::post, logout_url, viewer).result(), Status::no_content);
}

TEST_F(AdminApi, ManagesBansAndPaginatesTheirAuditEvents) {
    const auto administrator = sign_in();
    const auto status = call(Method::get, status_url, administrator);
    ASSERT_EQ(status.result(), Status::ok);
    EXPECT_EQ(Json::parse(status.body()).at("online_users"), 0);
    const auto added = call(Method::post, bans_url, administrator, ban_json);
    ASSERT_EQ(added.result(), Status::created);
    const auto id = Json::parse(added.body()).at("id").get<std::string>();
    const auto next =
        call(Method::post, bans_url, administrator,
             R"({"kind":"ip","identifier":"192.0.2.11","reason":"Timed","duration_seconds":300})");
    ASSERT_EQ(next.result(), Status::created);
    EXPECT_EQ(
        call(Method::post, bans_url, administrator,
             R"({"kind":"ip","identifier":"192.0.2.11","reason":"Bad","duration_seconds":-1})")
            .result(),
        Status::bad_request);
    EXPECT_EQ(call(Method::post, bans_url, administrator,
                   R"({"kind":"ip","identifier":"bad","reason":"Invalid"})")
                  .result(),
              Status::bad_request);
    const auto page = Json::parse(call(Method::get, "/api/v1/bans?limit=1", administrator).body());
    ASSERT_EQ(page.at("items").size(), 1U);
    EXPECT_EQ(page.at("items").front().at("id"), id);
    EXPECT_EQ(page.at("next_cursor"), id);
    const auto following =
        Json::parse(call(Method::get, "/api/v1/bans?limit=1&after=" + id, administrator).body());
    ASSERT_EQ(following.at("items").size(), 1U);
    EXPECT_EQ(following.at("items").front().at("identifier"), "192.0.2.11");
    EXPECT_FALSE(following.at("items").front().at("expires_utc").is_null());
    EXPECT_TRUE(following.at("next_cursor").is_null());
    EXPECT_EQ(call(Method::get, "/api/v1/bans?limit=101", administrator).result(),
              Status::bad_request);
    EXPECT_EQ(call(Method::delete_, "/api/v1/bans/invalid", administrator).result(),
              Status::bad_request);
    EXPECT_EQ(call(Method::delete_, "/api/v1/bans/" + id, administrator).result(),
              Status::no_content);
    EXPECT_EQ(call(Method::delete_, "/api/v1/bans/" + id, administrator).result(),
              Status::not_found);

    const auto page_one =
        Json::parse(call(Method::get, "/api/v1/audit?limit=1", administrator).body());
    const auto after = page_one.at("next_cursor").get<std::string>();
    const auto rest = call(Method::get, "/api/v1/audit?after=" + after, administrator);
    const auto events = Json::parse(rest.body()).at("items");
    ASSERT_GE(events.size(), 4U);
    EXPECT_EQ(events.front().at("type"), "ban.added");
    EXPECT_EQ(events.front().at("actor"), "admin-user");
    EXPECT_EQ(events.front().at("object_id"), id);
    EXPECT_NE(rest.body().find("ban.removed"), std::string::npos);
    EXPECT_EQ(rest.body().find("Fixture only"), std::string::npos);
    EXPECT_EQ(rest.body().find(administrator.csrf), std::string::npos);
}

TEST_F(AdminApi, ReturnsBoundedPagesOfLiveUsers) {
    const auto viewer = sign_in("viewer");
    RawClient first, second;
    ASSERT_FALSE(first.connect(adc()));
    const auto sid = enter(first, "node-one");
    ASSERT_TRUE(first.line().starts_with("BINF " + sid));
    ASSERT_FALSE(second.connect(adc()));
    const auto other_sid = enter(second, "node-two", "ZXO4VT7KPNYLJBLFLOR5YP3A33SPNOHYMEDJ4MY",
                                 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    EXPECT_TRUE(second.line().starts_with("BINF " + sid));
    EXPECT_TRUE(second.line().starts_with("BINF " + other_sid));
    const auto first_page = Json::parse(call(Method::get, "/api/v1/users?limit=1", viewer).body());
    ASSERT_EQ(first_page.at("items").size(), 1U);
    EXPECT_EQ(first_page.at("items").front().at("nickname"), "node-one");
    EXPECT_EQ(first_page.at("next_cursor"), sid);
    const auto second_page =
        Json::parse(call(Method::get, "/api/v1/users?after=" + sid, viewer).body());
    ASSERT_EQ(second_page.at("items").size(), 1U);
    EXPECT_EQ(second_page.at("items").front().at("sid"), other_sid);
    EXPECT_TRUE(second_page.at("next_cursor").is_null());
    EXPECT_EQ(call(Method::get, "/api/v1/users?after=invalid", viewer).result(),
              Status::bad_request);
}

TEST_F(AdminApi, ReportsDatabaseErrorsWhileTheHubContinuesToRouteChat) {
    const auto administrator = sign_in();
    RawClient client;
    ASSERT_FALSE(client.connect(adc()));
    const auto sid = enter(client, "node");
    ASSERT_TRUE(client.line().starts_with("BINF " + sid));
    TestSql sql;
    sql.execute("LOCK TABLES admin_sessions WRITE");
    auto http_result = std::async(std::launch::async,
                                  [&] { return call(Method::get, status_url, administrator); });
    std::this_thread::sleep_for(100ms);
    const auto message = "BMSG " + sid + " online\n";
    client.send(message);
    EXPECT_EQ(client.line(), message);
    EXPECT_EQ(http_result.wait_for(0ms), std::future_status::timeout);
    EXPECT_EQ(http_result.get().result(), Status::service_unavailable);
    sql.execute("UNLOCK TABLES");
    std::filesystem::remove(socket_);
    const auto outage = call(Method::get, status_url, administrator);
    EXPECT_EQ(outage.result(), Status::service_unavailable);
    EXPECT_EQ(code(outage), "unavailable");
    EXPECT_EQ(call(Method::get, status_url).result(), Status::unauthorized);
    std::filesystem::create_symlink(fixture_path("mdb.sock"), socket_);
    EXPECT_EQ(call(Method::get, status_url, administrator).result(), Status::ok);
}

} // namespace
} // namespace dc24h::admin
