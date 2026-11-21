#include "dc24h/db/store.hpp"

#include "../adc/test_messages.hpp"
#include "../net/test_certificate.hpp"
#include "../net/test_client.hpp"

#include <gtest/gtest.h>
#include <mysql.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace dc24h::db {
namespace {

namespace asio = boost::asio;
using namespace std::chrono_literals;
using net::test::RawClient;
using net::test::TestCertificate;
using net::test::TlsClient;

constexpr std::string_view other_pid = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
constexpr std::string_view other_cid = "ZXO4VT7KPNYLJBLFLOR5YP3A33SPNOHYMEDJ4MY";
constexpr std::string_view unavailable = "ISTA 210 Admission\\stemporarily\\sunavailable\n";

std::filesystem::path fixture_path(std::string_view name) {
    const char *directory = std::getenv("DC24H_TEST_DB_DIR");
    if (!directory || !std::string_view(directory).starts_with("/tmp/dc24h-e5.")) {
        throw std::runtime_error("Run this test suite through the isolated MariaDB fixture");
    }
    return std::filesystem::path(directory) / name;
}

Config credentials(std::string_view role, std::string_view database = "dc24h_e5_test") {
    std::ifstream file(fixture_path(std::string(role) + ".pw"));
    std::string password;
    if (!std::getline(file, password) || password.empty()) {
        throw std::runtime_error("Could not read fixture credentials");
    }
    return {std::string(database), "e5_" + std::string(role), std::move(password),
            fixture_path("mdb.sock")};
}

class TestSql {
  public:
    explicit TestSql(const char *database = "dc24h_e5_test") {
        if (!mysql_real_connect(connection_.get(), "localhost", "root", "", database, 0,
                                fixture_path("mdb.sock").c_str(), 0)) {
            throw std::runtime_error("Could not connect to the fixture as its administrator");
        }
    }

    void execute(const char *sql) {
        if (mysql_query(connection_.get(), sql) != 0) {
            throw std::runtime_error(mysql_error(connection_.get()));
        }
        if (auto *result = mysql_store_result(connection_.get())) {
            mysql_free_result(result);
        }
    }

  private:
    std::unique_ptr<MYSQL, decltype(&mysql_close)> connection_{mysql_init(nullptr), &mysql_close};
};

TEST(MariaDbMigrations, ResumesAfterDdlOrValidationFailureAndKeepsRows) {
    const auto config = credentials("migrator", "dc24h_e5_upgrade");
    TestSql sql("dc24h_e5_upgrade");
    sql.execute("CREATE TABLE bans (unexpected INT) ENGINE=InnoDB");
    EXPECT_THROW(static_cast<void>(migrate(config)), Error);

    Store store(config);
    EXPECT_EQ(store.schema_version(), 1U);
    EXPECT_THROW(store.require_current_schema(), Error);
    sql.execute("INSERT INTO hub_settings (name, value) VALUES ('max-clients', '12')");
    sql.execute("DROP TABLE bans");
    EXPECT_EQ(migrate(config), current_schema);
    EXPECT_EQ(store.settings(), (Settings{{"max-clients", "12"}}));

    // Simulate an interruption after the table was created but before its version was recorded.
    sql.execute("INSERT INTO audit_events (event_type, details) VALUES ('fixture', 'preserve')");
    sql.execute("DELETE FROM schema_migrations WHERE version = 3");
    EXPECT_EQ(store.schema_version(), 2U);
    EXPECT_EQ(migrate(config), current_schema);
    EXPECT_EQ(migrate(config), current_schema);
    EXPECT_EQ(store.settings(), (Settings{{"max-clients", "12"}}));

    // The migrator has SELECT on the database but does not need operational privileges.
    const auto events = store.audit_events();
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events.front().details, "preserve");
    sql.execute("UPDATE schema_migrations SET name = 'unknown' WHERE version = 3");
    EXPECT_THROW(static_cast<void>(migrate(config)), Error);
    EXPECT_THROW(store.require_current_schema(), Error);
}

TEST(MariaDbMigrations, SerializesConcurrentMigrators) {
    TestSql sql;
    sql.execute("SELECT GET_LOCK(CONCAT('dc24h:', LEFT(SHA2(LOWER(DATABASE()), 256), 56)), 0)");
    const auto config = credentials("migrator");
    EXPECT_THROW(static_cast<void>(migrate(config)), Error);
    sql.execute("SELECT RELEASE_ALL_LOCKS()");
    EXPECT_EQ(migrate(config), current_schema);
}

class MariaDbPersistence : public testing::Test {
  protected:
    void SetUp() override {
        TestSql sql;
        sql.execute("DROP TRIGGER IF EXISTS reject_audit");
        sql.execute("TRUNCATE TABLE hub_settings");
        sql.execute("TRUNCATE TABLE bans");
        sql.execute("TRUNCATE TABLE audit_events");
    }
};

TEST_F(MariaDbPersistence, StoresSettingsAndAuditsWithoutSensitiveFields) {
    const auto config = credentials("admin");
    {
        Store store(config);
        EXPECT_TRUE(store.settings().empty());
        store.set_setting("max-message-bytes", "128", "operator.1");
        store.set_setting("max-message-bytes", "256", "operator.1");
        store.set_setting("hub-name", "Private fixture's value", "operator.2");
        EXPECT_THROW(store.set_setting("password", "private-fixture-token"), std::invalid_argument);
        EXPECT_THROW(store.set_setting("max-clients", "0"), std::invalid_argument);
        EXPECT_THROW(store.set_setting("hub-name", "other name", "secret actor"),
                     std::invalid_argument);
    }

    Store store(config);
    const Settings expected{{"hub-name", "Private fixture's value"}, {"max-message-bytes", "256"}};
    EXPECT_EQ(store.settings(), expected);
    const auto events = store.audit_events();
    ASSERT_EQ(events.size(), 6U);
    EXPECT_EQ(events[1].type, "setting.updated");
    EXPECT_EQ(events[1].actor, "operator.1");
    EXPECT_EQ(events[1].object_id, "max-message-bytes");
    EXPECT_EQ(events[1].details, "value saved");
    EXPECT_EQ(events[1].created_at.size(), 26U);
    EXPECT_EQ(events[2].actor, "operator.2");
    EXPECT_EQ(events[2].object_id, "hub-name");
    for (std::size_t index = 3; index < events.size(); ++index) {
        EXPECT_EQ(events[index].type, "admin.failed");
        EXPECT_FALSE(events[index].actor);
        EXPECT_FALSE(events[index].object_id);
        EXPECT_EQ(events[index].details, "settings.set invalid_input");
    }
    EXPECT_EQ(store.audit_events(events[4].id).size(), 1U);
}

TEST_F(MariaDbPersistence, CanonicalizesAndExpiresBansAndAuditsTheirIds) {
    const auto config = credentials("admin");
    std::uint64_t timed_id = 0;
    std::uint64_t ip_id = 0;
    const std::string reason = "Fixture: user's private reason";
    {
        Store store(config);
        EXPECT_THROW(static_cast<void>(store.add_ban({BanKind::cid, "invalid", reason, {}})),
                     std::invalid_argument);
        EXPECT_THROW(static_cast<void>(store.add_ban({BanKind::ip, "127.0.0.0/24", reason, {}})),
                     std::invalid_argument);
        EXPECT_THROW(static_cast<void>(store.add_ban({BanKind::ip, "fe80::1%host", reason, {}})),
                     std::invalid_argument);
        EXPECT_THROW(static_cast<void>(store.add_ban({BanKind::ip, "::1", "bad\nreason", {}})),
                     std::invalid_argument);
        EXPECT_THROW(static_cast<void>(store.add_ban({BanKind::ip, "::1", reason, 0})),
                     std::invalid_argument);
        timed_id = store.add_ban({BanKind::cid, adc::test::cid, reason, 2}, "moderator");
        ip_id = store.add_ban({BanKind::ip, "::ffff:127.0.0.2", reason, {}});
    }

    Store store(config);
    const auto bans = store.bans();
    ASSERT_EQ(bans.size(), 2U);
    EXPECT_EQ(bans[0].id, timed_id);
    EXPECT_EQ(bans[0].reason, reason);
    EXPECT_EQ(bans[0].actor, "moderator");
    EXPECT_TRUE(bans[0].expires_at);
    EXPECT_EQ(bans[1].id, ip_id);
    EXPECT_EQ(bans[1].identifier, "127.0.0.2");
    EXPECT_FALSE(bans[1].expires_at);
    EXPECT_EQ(store.bans(timed_id).size(), 1U);

    const auto timed = store.active_ban(adc::test::cid, "::1");
    ASSERT_TRUE(timed && timed->seconds_remaining);
    EXPECT_GT(*timed->seconds_remaining, 0U);
    const auto permanent = store.active_ban(adc::test::cid, "127.0.0.2");
    ASSERT_TRUE(permanent);
    EXPECT_FALSE(permanent->seconds_remaining);
    EXPECT_TRUE(store.active_ban(other_cid, "::ffff:127.0.0.2"));
    EXPECT_FALSE(store.active_ban(other_cid, "127.0.0.1"));

    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline &&
           store.active_ban(adc::test::cid, "127.0.0.1")) {
        std::this_thread::sleep_for(50ms);
    }
    EXPECT_FALSE(store.active_ban(adc::test::cid, "127.0.0.1"));
    EXPECT_EQ(store.bans().size(), 2U);

    EXPECT_TRUE(store.remove_ban(ip_id, "moderator"));
    EXPECT_FALSE(store.remove_ban(ip_id, "moderator"));
    EXPECT_FALSE(store.active_ban(other_cid, "127.0.0.2"));
    const auto events = store.audit_events();
    ASSERT_EQ(events.size(), 9U);
    EXPECT_EQ(events[6].type, "ban.added");
    EXPECT_EQ(events[6].object_id, std::to_string(ip_id));
    EXPECT_EQ(events[6].details, "ip");
    EXPECT_EQ(events[7].type, "ban.removed");
    EXPECT_EQ(events[7].object_id, std::to_string(ip_id));
    EXPECT_EQ(events[8].type, "admin.failed");
    EXPECT_EQ(events[8].details, "bans.remove not_found");
}

TEST_F(MariaDbPersistence, RollsBackMutationsWhenTheAuditWriteFails) {
    TestSql sql;
    sql.execute("CREATE TRIGGER reject_audit BEFORE INSERT ON audit_events FOR EACH ROW "
                "SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'fixture opaque credential'");
    Store store(credentials("admin"));
    try {
        store.set_setting("hub-name", "Private value");
        ADD_FAILURE() << "The setting was saved without an audit event";
    } catch (const Error &error) {
        EXPECT_EQ(std::string_view(error.what()).find("fixture opaque credential"),
                  std::string_view::npos);
    }
    EXPECT_TRUE(store.settings().empty());
    EXPECT_THROW(static_cast<void>(store.add_ban({BanKind::ip, "::1", "fixture", {}})), Error);
    EXPECT_TRUE(store.bans().empty());
    sql.execute("DROP TRIGGER reject_audit");
    EXPECT_GT(store.add_ban({BanKind::ip, "::1", "fixture", {}}), 0U);
}

net::ServerConfig listener_config(const TestCertificate &certificate) {
    net::ServerConfig config;
    config.certificate = certificate.cert_path;
    config.private_key = certificate.key_path;
    config.adc = net::Endpoint{"127.0.0.1", 0};
    config.adcs.port = 0;
    config.limits.shutdown_timeout = 250ms;
    return config;
}

class RunningHub {
  public:
    RunningHub(const TestCertificate &certificate, Config config)
        : server_(io_, listener_config(certificate), {}, std::move(config)), worker_([this] {
              try {
                  io_.run();
              } catch (const std::exception &error) {
                  ADD_FAILURE() << "Hub I/O worker failed: " << error.what();
              }
          }) {}

    ~RunningHub() {
        server_.stop();
        worker_.join();
    }

    const net::Endpoint &adc() const { return server_.adc_endpoint().value(); }
    const net::Endpoint &adcs() const { return server_.adcs_endpoint(); }

  private:
    asio::io_context io_;
    net::Server server_;
    std::jthread worker_;
};

template <typename Client> std::string negotiate(Client &client) {
    client.send("HSUP ADBASE ADTIGR\n");
    EXPECT_EQ(client.line(), "ISUP ADTIGR ADBASE\n");
    const auto frame = adc::test::decode(client.line());
    const auto sid = adc::parse_sid(frame);
    const auto result = sid ? std::string(*sid) : std::string{};
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(client.line().starts_with("IINF "));
    return result;
}

template <typename Client>
void request_admission(Client &client, const std::string &sid,
                       std::string_view cid = adc::test::cid,
                       std::string_view pid = adc::test::pid) {
    client.send("BINF " + sid + " ID" + std::string(cid) + " PD" + std::string(pid) + " NI" + sid +
                " I4192.0.2.10\n");
}

void expect_chat(RawClient &client, const std::string &sid) {
    const auto message = "BMSG " + sid + " still\\sonline\n";
    client.send(message);
    EXPECT_EQ(client.line(), message);
}

TEST_F(MariaDbPersistence, EnforcesPeerIpAndVerifiedCidAcrossListenersAndRestarts) {
    const TestCertificate certificate;
    Store admin(credentials("admin"));
    const auto ip_ban = admin.add_ban({BanKind::ip, "127.0.0.1", "private IP reason", {}});
    static_cast<void>(admin.add_ban({BanKind::cid, adc::test::cid, "private CID reason", 600}));
    {
        RunningHub hub(certificate, credentials("reader"));
        RawClient client;
        ASSERT_FALSE(client.connect(hub.adc()));
        const auto sid = negotiate(client);
        request_admission(client, sid, other_cid, other_pid);
        EXPECT_EQ(client.line(), "ISTA 231 Permanently\\sbanned\n");
    }
    ASSERT_TRUE(admin.remove_ban(ip_ban));

    RunningHub hub(certificate, credentials("reader"));
    TlsClient rejected(certificate.cert_path);
    ASSERT_FALSE(rejected.connect(hub.adcs()));
    ASSERT_FALSE(rejected.handshake());
    const auto sid = negotiate(rejected);
    request_admission(rejected, sid);
    const auto frame = adc::test::decode(rejected.line());
    const auto reply = adc::parse_status(frame);
    ASSERT_TRUE(reply);
    EXPECT_EQ(reply->severity, adc::Severity::fatal);
    EXPECT_EQ(reply->code, 32U);
    ASSERT_EQ(reply->fields.size(), 1U);
    EXPECT_EQ(reply->fields.front().name, "TL");

    RawClient allowed;
    ASSERT_FALSE(allowed.connect(hub.adc()));
    const auto allowed_sid = negotiate(allowed);
    request_admission(allowed, allowed_sid, other_cid, other_pid);
    EXPECT_TRUE(allowed.line().starts_with("BINF " + allowed_sid));
}

TEST_F(MariaDbPersistence, RefusesNewUsersDuringInitialOutageAndAfterConnectionLoss) {
    const TestCertificate certificate;
    auto config = credentials("reader");
    const auto socket = fixture_path("alternate.sock");
    std::filesystem::remove(socket);
    config.socket = socket;
    RunningHub hub(certificate, config);
    {
        RawClient denied;
        ASSERT_FALSE(denied.connect(hub.adc()));
        request_admission(denied, negotiate(denied));
        EXPECT_EQ(denied.line(), unavailable);
    }
    std::filesystem::create_symlink(fixture_path("mdb.sock"), socket);
    std::this_thread::sleep_for(300ms);
    RawClient online;
    ASSERT_FALSE(online.connect(hub.adc()));
    const auto sid = negotiate(online);
    request_admission(online, sid);
    ASSERT_TRUE(online.line().starts_with("BINF " + sid));

    std::filesystem::remove(socket);
    TestSql sql;
    sql.execute("KILL USER e5_reader");
    RawClient denied;
    ASSERT_FALSE(denied.connect(hub.adc()));
    request_admission(denied, negotiate(denied), other_cid, other_pid);
    EXPECT_EQ(denied.line(), unavailable);
    expect_chat(online, sid);

    std::filesystem::create_symlink(fixture_path("mdb.sock"), socket);
    std::this_thread::sleep_for(300ms);
    RawClient recovered;
    ASSERT_FALSE(recovered.connect(hub.adc()));
    const auto recovered_sid = negotiate(recovered);
    request_admission(recovered, recovered_sid, other_cid, other_pid);
    EXPECT_TRUE(recovered.line().starts_with("BINF " + sid));
    EXPECT_TRUE(recovered.line().starts_with("BINF " + recovered_sid));
}

TEST_F(MariaDbPersistence, TimesOutBlockedQueriesWhileOneIoWorkerRoutesChat) {
    const TestCertificate certificate;
    RunningHub hub(certificate, credentials("reader"));
    RawClient online, pending;
    ASSERT_FALSE(online.connect(hub.adc()));
    const auto online_sid = negotiate(online);
    request_admission(online, online_sid);
    ASSERT_TRUE(online.line().starts_with("BINF " + online_sid));
    ASSERT_FALSE(pending.connect(hub.adc()));
    const auto pending_sid = negotiate(pending);

    TestSql sql;
    sql.execute("LOCK TABLES bans WRITE");
    const auto started = std::chrono::steady_clock::now();
    request_admission(pending, pending_sid, other_cid, other_pid);
    // Give the blocked check time to reach MariaDB before testing the network loop.
    std::this_thread::sleep_for(100ms);
    expect_chat(online, online_sid);
    EXPECT_LT(std::chrono::steady_clock::now() - started, 900ms);
    EXPECT_EQ(pending.line(), unavailable);
    EXPECT_GE(std::chrono::steady_clock::now() - started, 900ms);
    sql.execute("UNLOCK TABLES");
    expect_chat(online, online_sid);
}

} // namespace
} // namespace dc24h::db
