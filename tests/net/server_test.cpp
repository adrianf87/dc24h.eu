#include "test_certificate.hpp"
#include "test_client.hpp"

#include "../adc/test_messages.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace dc24h::net {
namespace {

using namespace std::chrono_literals;
using test::Error;
using test::RawClient;
using test::TestCertificate;
using test::TlsClient;
namespace asio = boost::asio;

template <typename Predicate> bool wait_for(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 4s;
    do {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(2ms);
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

ServerConfig loopback_config(const TestCertificate &certificate, bool enable_adc = true) {
    ServerConfig config;
    config.adcs.port = 0;
    if (enable_adc) {
        config.adc = Endpoint{"127.0.0.1", 0};
    }
    config.certificate = certificate.cert_path;
    config.private_key = certificate.key_path;
    config.limits.shutdown_timeout = 350ms;
    return config;
}

class RunningServer {
  public:
    explicit RunningServer(ServerConfig config)
        : server_(io_, std::move(config), [this](std::string_view event) {
              const std::lock_guard guard(log_mutex_);
              log_.emplace_back(event);
          }) {
        try {
            for (int index = 0; index < 2; ++index) {
                workers_.emplace_back([this] {
                    try {
                        io_.run();
                    } catch (const std::exception &error) {
                        ADD_FAILURE() << "Server worker failed: " << error.what();
                        io_.stop();
                    }
                });
            }
        } catch (...) {
            io_.stop();
            throw;
        }
    }

    ~RunningServer() {
        stop();
        join();
    }

    const Endpoint &adc() const { return server_.adc_endpoint().value(); }

    const Endpoint &adcs() const { return server_.adcs_endpoint(); }

    bool adc_enabled() const { return server_.adc_endpoint().has_value(); }

    bool active_is(std::size_t expected) const {
        return wait_for([&] { return server_.active_clients() == expected; });
    }

    bool online_is(std::size_t expected) const {
        return wait_for([&] { return server_.online_users() == expected; });
    }

    bool logged(std::string_view message) {
        return wait_for([&] {
            const std::lock_guard guard(log_mutex_);
            return std::ranges::any_of(log_, [message](const auto &line) {
                return line.find(message) != std::string::npos;
            });
        });
    }

    void stop() { server_.stop(); }

    void join() {
        for (auto &worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

  private:
    std::mutex log_mutex_;
    std::vector<std::string> log_;
    asio::io_context io_;
    Server server_;
    std::vector<std::jthread> workers_;
};

template <typename Client> std::string read_support(Client &client) {
    EXPECT_EQ(client.line(), "ISUP ADTIGR ADBASE\n");
    const auto sid_frame = adc::test::decode(client.line());
    EXPECT_EQ(sid_frame.fourcc(), "ISID");
    EXPECT_TRUE(client.line().starts_with("IINF CT32 NIdc24h.eu VE"));
    return std::string(adc::parse_sid(sid_frame).value());
}

template <typename Client> std::string negotiate(Client &client) {
    client.send("HSUP ADBASE ADTIGR\n");
    return read_support(client);
}

std::string identification(std::string_view sid, std::string_view cid = adc::test::cid,
                           std::string_view pid = adc::test::pid, std::string_view nick = {},
                           std::string_view features = "BASE") {
    std::string wire = "BINF ";
    wire += sid;
    wire += " ID";
    wire += cid;
    wire += " PD";
    wire += pid;
    wire += " NI";
    wire += nick.empty() ? sid : nick;
    wire += " SU";
    wire += features;
    wire += " I4192.0.2.9\n";
    return wire;
}

void expect_info_fields(const adc::Message &info, std::string_view sid, std::string_view cid,
                        std::string_view nick) {
    EXPECT_EQ(info.fourcc(), "BINF");
    EXPECT_EQ(info.sender_sid, sid);
    const auto fields = adc::parse_info(info);
    ASSERT_TRUE(fields);
    EXPECT_TRUE(std::ranges::none_of(*fields, [](const adc::Field &item) {
        return item.name == "PD" || item.name == "EM" || item.name == "RF" || item.name == "TO" ||
               item.name == "CT" || item.name == "ZZ";
    }));
    EXPECT_TRUE(std::ranges::any_of(
        *fields, [cid](const adc::Field &item) { return item.name == "ID" && item.value == cid; }));
    const auto expected_nick = nick.empty() ? sid : nick;
    EXPECT_TRUE(std::ranges::any_of(*fields, [expected_nick](const adc::Field &item) {
        return item.name == "NI" && item.value == expected_nick;
    }));
    EXPECT_TRUE(std::ranges::any_of(*fields, [](const adc::Field &item) {
        return item.name == "I4" && item.value == "127.0.0.1";
    }));
}

template <typename Client>
void expect_info(Client &client, std::string_view sid, std::string_view cid = adc::test::cid,
                 std::string_view nick = {}) {
    expect_info_fields(adc::test::decode(client.line()), sid, cid, nick);
}

template <typename Client> void identify(Client &client, std::string_view sid) {
    client.send(identification(sid));
    expect_info(client, sid);
}

constexpr std::string_view second_pid = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
constexpr std::string_view second_cid = "ZXO4VT7KPNYLJBLFLOR5YP3A33SPNOHYMEDJ4MY";

bool socket_closed(const Error &error) {
    return error == asio::error::eof || error == asio::error::connection_reset;
}

TEST(AdcServer, DefaultsToAdcsAndStopsListening) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate, false));
    EXPECT_FALSE(server.adc_enabled());
    EXPECT_GT(server.adcs().port, 0U);
    server.stop();
    server.join();
    RawClient probe;
    EXPECT_EQ(probe.connect(server.adcs()), asio::error::connection_refused);
}

TEST(AdcServer, ValidatesTlsCredentialsAndListenerSettingsAtStartup) {
    const TestCertificate first;
    const TestCertificate second;
    auto config = loopback_config(first);
    config.private_key = second.key_path;
    asio::io_context io;
    EXPECT_THROW(static_cast<void>(Server(io, config)), std::exception);
    config = loopback_config(first);
    config.adcs.port = config.adc->port = 1511;
    EXPECT_THROW(static_cast<void>(Server(io, config)), std::invalid_argument);
    config = loopback_config(first);
    config.adcs.address = "localhost";
    EXPECT_THROW(static_cast<void>(Server(io, config)), std::invalid_argument);
}

TEST(AdcServer, TwoPlainClientsCompleteIndependentHandshakes) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    ASSERT_NE(server.adc().port, server.adcs().port);
    RawClient first;
    RawClient second;
    ASSERT_FALSE(first.connect(server.adc()));
    ASSERT_FALSE(second.connect(server.adc()));
    ASSERT_TRUE(server.active_is(2));
    const auto first_sid = negotiate(first);
    const auto second_sid = negotiate(second);
    EXPECT_NE(first_sid, second_sid);
    identify(first, first_sid);
    second.send(identification(second_sid, second_cid, second_pid));
    expect_info(second, first_sid);
    expect_info(second, second_sid, second_cid);
    expect_info(first, second_sid, second_cid);
    first.close();
    second.close();
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, TwoAdcsClientsCompleteAdcHandshakesAndCleanTlsShutdown) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    TlsClient first(certificate.cert_path, TLS1_2_VERSION);
    TlsClient second(certificate.cert_path, TLS1_3_VERSION);
    ASSERT_FALSE(first.connect(server.adcs()));
    ASSERT_FALSE(second.connect(server.adcs()));
    ASSERT_FALSE(first.handshake());
    ASSERT_FALSE(second.handshake());
    ASSERT_TRUE(server.active_is(2));
    const auto first_sid = negotiate(first);
    const auto second_sid = negotiate(second);
    EXPECT_NE(first_sid, second_sid);
    identify(first, first_sid);
    second.send(identification(second_sid, second_cid, second_pid));
    expect_info(second, first_sid);
    expect_info(second, second_sid, second_cid);
    expect_info(first, second_sid, second_cid);
    EXPECT_FALSE(first.shutdown_tls());
    EXPECT_EQ(second.line(), "IQUI " + first_sid + '\n');
    EXPECT_FALSE(second.shutdown_tls());
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, RejectsUntrustedAndObsoleteTlsHandshakes) {
    const TestCertificate certificate;
    const TestCertificate unrelated;
    RunningServer server(loopback_config(certificate));
    TlsClient untrusted(unrelated.cert_path);
    ASSERT_FALSE(untrusted.connect(server.adcs()));
    EXPECT_TRUE(untrusted.handshake());
    EXPECT_TRUE(server.logged("TLS handshake failed"));
    EXPECT_TRUE(server.active_is(0));

    TlsClient obsolete(certificate.cert_path, TLS1_VERSION);
    ASSERT_FALSE(obsolete.connect(server.adcs()));
    EXPECT_TRUE(obsolete.handshake());
    obsolete.close();
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, RejectsPlaintextOnAdcsEvenWhenAdcIsEnabled) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    RawClient plaintext;
    ASSERT_FALSE(plaintext.connect(server.adcs()));
    plaintext.send("HSUP ADBASE ADTIGR\n");
    std::string bytes;
    EXPECT_TRUE(socket_closed(plaintext.read_to_close(bytes)));
    EXPECT_EQ(bytes.find("ISUP"), std::string::npos);
    EXPECT_EQ(bytes.find("ISID"), std::string::npos);
    EXPECT_TRUE(server.logged("TLS handshake failed"));
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, DrainsFatalAdcStatusBeforeClosingTls) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    TlsClient client(certificate.cert_path);
    ASSERT_FALSE(client.connect(server.adcs()));
    ASSERT_FALSE(client.handshake());
    client.send("HSUP ADBASE\n");
    const auto message = adc::test::decode(client.line());
    const auto status = adc::parse_status(message);
    ASSERT_TRUE(status);
    EXPECT_EQ(status->severity, adc::Severity::fatal);
    EXPECT_EQ(status->code, 47U);
    EXPECT_FALSE(client.shutdown_tls());
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, ExpiresClientsDuringTlsHandshake) {
    const TestCertificate certificate;
    auto config = loopback_config(certificate);
    config.limits.tls_timeout = 150ms;
    RunningServer server(config);
    RawClient silent;
    ASSERT_FALSE(silent.connect(server.adcs()));
    std::string bytes;
    EXPECT_TRUE(socket_closed(silent.read_to_close(bytes)));
    EXPECT_TRUE(bytes.empty());
    EXPECT_TRUE(server.logged("TLS handshake timeout"));
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, EnforcesTheOverallAndIdentificationDeadlines) {
    const TestCertificate certificate;
    auto config = loopback_config(certificate);
    config.limits.setup_timeout = 250ms;
    RunningServer bounded(config);
    RawClient slow_tls;
    ASSERT_FALSE(slow_tls.connect(bounded.adcs()));
    std::string data;
    EXPECT_TRUE(socket_closed(slow_tls.read_to_close(data)));
    EXPECT_TRUE(bounded.logged("Connection setup timeout"));

    config.limits.setup_timeout = 3s;
    config.limits.identify_timeout = 250ms;
    RunningServer bounded_identification(config);
    RawClient no_inf;
    ASSERT_FALSE(no_inf.connect(bounded_identification.adc()));
    EXPECT_TRUE(adc::is_sid(negotiate(no_inf)));
    EXPECT_TRUE(socket_closed(no_inf.read_to_close(data)));
    EXPECT_TRUE(bounded_identification.logged("ADC identification timeout"));
}

TEST(AdcServer, RecoversFromMalformedAndFragmentedFrames) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    RawClient client;
    ASSERT_FALSE(client.connect(server.adc()));
    client.send("HSUP ADTI\\x\nHS");
    client.send("UP ADBASE ADTIGR\n");
    const auto sid = read_support(client);
    EXPECT_TRUE(server.logged("Discarded malformed ADC frame"));
    identify(client, sid);
}

TEST(AdcServer, AppliesOneClientLimitAndReleasesSlotsAcrossListeners) {
    const TestCertificate certificate;
    auto config = loopback_config(certificate);
    config.max_clients = 1;
    RunningServer server(config);
    RawClient first;
    ASSERT_FALSE(first.connect(server.adc()));
    const auto previous_sid = negotiate(first);
    ASSERT_TRUE(server.active_is(1));

    RawClient excess;
    ASSERT_FALSE(excess.connect(server.adcs()));
    std::string ignored;
    EXPECT_TRUE(socket_closed(excess.read_to_close(ignored)));
    EXPECT_TRUE(server.logged("Connection refused at the client limit"));
    first.close();
    ASSERT_TRUE(server.active_is(0));

    TlsClient replacement(certificate.cert_path);
    ASSERT_FALSE(replacement.connect(server.adcs()));
    ASSERT_FALSE(replacement.handshake());
    const auto new_sid = negotiate(replacement);
    EXPECT_NE(previous_sid, new_sid);
    identify(replacement, new_sid);
    EXPECT_FALSE(replacement.shutdown_tls());
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, ClosesOnInputAndPendingOutputLimits) {
    const TestCertificate certificate;
    auto config = loopback_config(certificate);
    config.limits.input_bytes = 64;
    config.limits.pending_bytes = 16;
    RunningServer server(config);

    RawClient oversized;
    ASSERT_FALSE(oversized.connect(server.adc()));
    oversized.send(std::string(65, 'X'));
    std::string data;
    EXPECT_TRUE(socket_closed(oversized.read_to_close(data)));
    EXPECT_TRUE(server.logged("ADC input limit exceeded"));

    RawClient small_queue;
    ASSERT_FALSE(small_queue.connect(server.adc()));
    small_queue.send("HSUP ADBASE ADTIGR\n");
    EXPECT_TRUE(socket_closed(small_queue.read_to_close(data)));
    EXPECT_TRUE(server.logged("Pending output limit exceeded"));
    EXPECT_TRUE(data.empty());
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, IdleExpirySendsQuitAndReleasesTheSession) {
    const TestCertificate certificate;
    auto config = loopback_config(certificate);
    config.limits.idle_timeout = 200ms;
    RunningServer server(config);
    RawClient client;
    ASSERT_FALSE(client.connect(server.adc()));
    const auto sid = negotiate(client);
    identify(client, sid);
    EXPECT_EQ(client.line(), "IQUI " + sid + '\n');
    EXPECT_TRUE(server.logged("Idle timeout"));
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, DiscardsIncompleteInputWhenPeerDisconnectsDuringRead) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    RawClient client;
    ASSERT_FALSE(client.connect(server.adc()));
    ASSERT_TRUE(server.active_is(1));
    client.send("HSUP AD");
    client.close();
    EXPECT_TRUE(server.logged("Discarded incomplete ADC frame at end of stream"));
    EXPECT_TRUE(server.active_is(0));
}

TEST(AdcServer, ReleasesTheSessionWhenPeerResetsDuringALargeReply) {
    const TestCertificate certificate;
    auto config = loopback_config(certificate);
    config.limits.input_bytes = 512 * 1024;
    config.limits.pending_bytes = 512 * 1024;
    RunningServer server(config);
    RawClient client;
    client.socket().open(test::tcp::v4());
    client.socket().set_option(asio::socket_base::receive_buffer_size(1024));
    ASSERT_FALSE(client.connect(server.adc()));
    const auto sid = negotiate(client);
    auto wire = identification(sid);
    wire.pop_back();
    for (const auto field : {"DE", "AP", "VE"}) {
        wire += ' ';
        wire += field;
        wire.append(4000, 'x');
    }
    wire += '\n';
    client.send(wire);
    EXPECT_EQ(client.read_prefix(5), "BINF ");
    client.reset();
    ASSERT_TRUE(server.active_is(0));

    RawClient next;
    ASSERT_FALSE(next.connect(server.adc()));
    identify(next, negotiate(next));
}

TEST(AdcServer, StopsAcceptingAndGracefullyClosesEstablishedSessions) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    RawClient plain;
    TlsClient encrypted(certificate.cert_path);
    ASSERT_FALSE(plain.connect(server.adc()));
    ASSERT_FALSE(encrypted.connect(server.adcs()));
    ASSERT_FALSE(encrypted.handshake());
    const auto adc_sid = negotiate(plain);
    const auto adcs_sid = negotiate(encrypted);
    identify(plain, adc_sid);
    encrypted.send(identification(adcs_sid, second_cid, second_pid));
    expect_info(encrypted, adc_sid);
    expect_info(encrypted, adcs_sid, second_cid);
    expect_info(plain, adcs_sid, second_cid);
    const auto chat = "BMSG " + adcs_sid + " hello\\sADC\n";
    encrypted.send(chat);
    EXPECT_EQ(plain.line(), chat);
    EXPECT_EQ(encrypted.line(), chat);
    server.stop();
    server.stop();
    EXPECT_EQ(plain.line(), "IQUI " + adc_sid + '\n');
    EXPECT_EQ(encrypted.line(), "IQUI " + adcs_sid + '\n');
    EXPECT_FALSE(encrypted.shutdown_tls());
    server.join();
    EXPECT_TRUE(server.active_is(0));
    EXPECT_TRUE(server.online_is(0));
    RawClient probe;
    EXPECT_EQ(probe.connect(server.adc()), asio::error::connection_refused);
}

TEST(AdcServer, ShutdownDeadlineClosesUncooperativeTlsPeers) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    TlsClient client(certificate.cert_path);
    ASSERT_FALSE(client.connect(server.adcs()));
    ASSERT_FALSE(client.handshake());
    identify(client, negotiate(client));
    server.stop();
    server.join();
    EXPECT_TRUE(server.logged("Shutdown timeout"));
    EXPECT_TRUE(server.active_is(0));
}

struct Identity {
    std::string_view cid;
    std::string_view pid;
};

// TIGR/192 of 24 bytes filled with the index of the vector.
constexpr Identity identities[] = {
    {second_cid, second_pid},
    {"2OAQNIXGXYDKKV5VMUWQJMEYEFQ64QPULDCLPTI", "AEAQCAIBAEAQCAIBAEAQCAIBAEAQCAIBAEAQCAI"},
    {"AFUZAFTMCBTPYNKXXS5K5XZCIFYWTHMZJOQE3RY", "AIBAEAQCAIBAEAQCAIBAEAQCAIBAEAQCAIBAEAQ"},
    {"KV5UVNFXISHNNQ4AE6WA5WJ52OPAAPXP4PAT3KY", "AMBQGAYDAMBQGAYDAMBQGAYDAMBQGAYDAMBQGAY"},
    {"O62VZNGB5AVT6MLAXDM7LS3HQXMPO6ROIZZ7RDA", "AQCAIBAEAQCAIBAEAQCAIBAEAQCAIBAEAQCAIBA"},
    {"65ECQ4TIQH3SBDLFJEAHIWQIUMY2WU4OWO4ATRY", "AUCQKBIFAUCQKBIFAUCQKBIFAUCQKBIFAUCQKBI"},
    {"U24ANPLVX2WN6XSS5CJ53ZANNDF4TQNZPFYYG2I", "AYDAMBQGAYDAMBQGAYDAMBQGAYDAMBQGAYDAMBQ"},
    {"PZDJP7WBILVOAOSCO3VROIJHL3XR7LMOVHO6ZIA", "A4DQOBYHA4DQOBYHA4DQOBYHA4DQOBYHA4DQOBY"},
    {"A5ZBRBD5VDQVZW4OOG3GSSBE6WHD4FO2VZSLQLY", "BAEAQCAIBAEAQCAIBAEAQCAIBAEAQCAIBAEAQCA"},
    {"2FEWBDVKBTUYP46ZNBJPUEOYCCOTMORITEUGW6Q", "BEEQSCIJBEEQSCIJBEEQSCIJBEEQSCIJBEEQSCI"},
};

TEST(AdcPresence, TenAdcsClientsTrackPresenceAndRouteChat) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate, false));
    constexpr auto count = std::size(identities);
    std::array<std::unique_ptr<TlsClient>, count> clients;
    std::array<std::string, count> sids;
    std::array<std::string, count> nicks;
    for (std::size_t index = 0; index < count; ++index) {
        clients[index] = std::make_unique<TlsClient>(certificate.cert_path);
        ASSERT_FALSE(clients[index]->connect(server.adcs()));
        ASSERT_FALSE(clients[index]->handshake());
        sids[index] = negotiate(*clients[index]);
        nicks[index] = "Client" + std::to_string(index);
    }
    ASSERT_TRUE(server.active_is(count));
    ASSERT_TRUE(server.online_is(0));
    ASSERT_EQ((std::set<std::string>{sids.begin(), sids.end()}.size()), count);
    clients[0]->send("BMSG " + sids[0] + " premature\n");
    EXPECT_TRUE(clients[0]->line().starts_with("ISTA 144 "));

    // Send all logins before consuming any roster. Every peer must see each SID once.
    for (std::size_t index = 0; index < count; ++index) {
        auto wire =
            identification(sids[index], identities[index].cid, identities[index].pid, nicks[index]);
        wire.pop_back();
        wire += " EMtest@example.invalid RFtest TOprivate CT4 ZZunknown\n";
        clients[index]->send(wire);
    }
    std::array<std::array<std::size_t, count>, count> positions{};
    for (std::size_t client = 0; client < count; ++client) {
        std::set<std::size_t> seen;
        for (std::size_t position = 0; position < count; ++position) {
            const auto info = adc::test::decode(clients[client]->line());
            ASSERT_TRUE(info.sender_sid);
            const auto sender = std::ranges::find(sids, *info.sender_sid);
            ASSERT_NE(sender, sids.end());
            const auto index = static_cast<std::size_t>(sender - sids.begin());
            ASSERT_TRUE(seen.insert(index).second);
            positions[client][index] = position;
            expect_info_fields(info, sids[index], identities[index].cid, nicks[index]);
        }
    }
    for (std::size_t client = 0; client < count; ++client) {
        for (std::size_t peer = 0; peer < count; ++peer) {
            EXPECT_EQ(positions[client][peer] < positions[client][client],
                      positions[peer][peer] < positions[client][client]);
        }
    }
    ASSERT_TRUE(server.online_is(count));

    const auto public_chat = "BMSG " + sids[0] + " Zażółć\\sgęślą\\njaźń\\\\ ME1\n";
    clients[0]->send(public_chat);
    for (auto &client : clients) {
        EXPECT_EQ(client->line(), public_chat);
    }

    const auto direct = "DMSG " + sids[0] + ' ' + sids[1] + " direct\\sbody PM" + sids[0] + '\n';
    const auto echo = "EMSG " + sids[0] + ' ' + sids[2] + " echo\\sbody PM" + sids[0] + '\n';
    const auto marker = "BMSG " + sids[0] + " after\\sprivate\n";
    clients[0]->send(direct + echo + "EMSG " + sids[0] + " 7777 hidden-body PM" + sids[0] + '\n' +
                     "DMSG " + sids[3] + ' ' + sids[2] + " spoof PM" + sids[3] + '\n' + "BMSG " +
                     sids[0] + " private-flag PM" + sids[0] + '\n' + marker);
    for (std::size_t index = 0; index < count; ++index) {
        if (index == 1) {
            EXPECT_EQ(clients[index]->line(), direct);
        }
        if (index == 0 || index == 2) {
            EXPECT_EQ(clients[index]->line(), echo);
        }
        if (index == 0) {
            EXPECT_EQ(clients[0]->line(), "ISTA 125 Recipient\\sis\\sunavailable FCEMSG\n");
            EXPECT_TRUE(clients[0]->line().starts_with("ISTA 125 "));
            EXPECT_TRUE(clients[0]->line().starts_with("ISTA 140 "));
        }
        // The next public message proves nonrecipients never received either private frame.
        EXPECT_EQ(clients[index]->line(), marker);
    }

    nicks[3] = "Renamed";
    const auto update = "BINF " + sids[3] + " NI" + nicks[3] + " SUBASE,TEST\n";
    const auto feature = "FMSG " + sids[3] + " +TEST-OLDX feature\n";
    const auto after_update = "BMSG " + sids[3] + " after\\supdate\n";
    clients[3]->send(update + feature + after_update);
    for (std::size_t index = 0; index < count; ++index) {
        EXPECT_EQ(clients[index]->line(), update);
        if (index == 3) {
            EXPECT_EQ(clients[index]->line(), feature);
        }
        EXPECT_EQ(clients[index]->line(), after_update);
    }

    constexpr std::size_t leaving = 5;
    const auto old_sid = sids[leaving];
    clients[leaving]->reset();
    for (std::size_t index = 0; index < count; ++index) {
        if (index != leaving) {
            EXPECT_EQ(clients[index]->line(), "IQUI " + old_sid + '\n');
        }
    }
    ASSERT_TRUE(server.online_is(count - 1));
    ASSERT_TRUE(server.active_is(count - 1));

    clients[leaving] = std::make_unique<TlsClient>(certificate.cert_path);
    ASSERT_FALSE(clients[leaving]->connect(server.adcs()));
    ASSERT_FALSE(clients[leaving]->handshake());
    sids[leaving] = negotiate(*clients[leaving]);
    ASSERT_NE(sids[leaving], old_sid);
    ASSERT_EQ((std::set<std::string>{sids.begin(), sids.end()}.size()), count);
    clients[leaving]->send(identification(sids[leaving], identities[leaving].cid,
                                          identities[leaving].pid, nicks[leaving]));
    std::set<std::string> reconnected_roster;
    for (std::size_t index = 0; index + 1 < count; ++index) {
        const auto peer = adc::test::decode(clients[leaving]->line());
        ASSERT_TRUE(peer.sender_sid);
        ASSERT_NE(*peer.sender_sid, sids[leaving]);
        const auto sender = std::ranges::find(sids, *peer.sender_sid);
        ASSERT_NE(sender, sids.end());
        const auto member = static_cast<std::size_t>(sender - sids.begin());
        ASSERT_TRUE(reconnected_roster.insert(*peer.sender_sid).second);
        expect_info_fields(peer, sids[member], identities[member].cid, nicks[member]);
    }
    expect_info(*clients[leaving], sids[leaving], identities[leaving].cid, nicks[leaving]);
    for (std::size_t index = 0; index < count; ++index) {
        if (index != leaving) {
            expect_info(*clients[index], sids[leaving], identities[leaving].cid, nicks[leaving]);
        }
    }
    ASSERT_TRUE(server.online_is(count));
    for (auto &client : clients) {
        client->close();
    }
    EXPECT_TRUE(server.active_is(0));
    EXPECT_TRUE(server.online_is(0));
}

TEST(AdcPresence, ConcurrentNicknameConflictHasOneWinnerAndOneDeparture) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    RawClient observer;
    RawClient first;
    RawClient second;
    ASSERT_FALSE(observer.connect(server.adc()));
    ASSERT_FALSE(first.connect(server.adc()));
    ASSERT_FALSE(second.connect(server.adc()));
    const auto observer_sid = negotiate(observer);
    const auto first_sid = negotiate(first);
    const auto second_sid = negotiate(second);
    identify(observer, observer_sid);
    first.send(identification(first_sid, identities[0].cid, identities[0].pid, "Straße"));
    second.send(identification(second_sid, identities[1].cid, identities[1].pid, "STRASSE"));
    const auto first_reply = first.line();
    const auto second_reply = second.line();
    const auto roster_line = "BINF " + observer_sid + ' ';
    const bool first_won = first_reply.starts_with(roster_line);
    ASSERT_NE(first_won, second_reply.starts_with(roster_line));
    EXPECT_TRUE((first_won ? second_reply : first_reply).starts_with("ISTA 222 "));
    auto &winner = first_won ? first : second;
    const auto &winner_sid = first_won ? first_sid : second_sid;
    const auto winner_cid = identities[first_won ? 0 : 1].cid;
    const auto winner_nick = first_won ? "Straße" : "STRASSE";
    expect_info(winner, winner_sid, winner_cid, winner_nick);
    expect_info(observer, winner_sid, winner_cid, winner_nick);
    ASSERT_TRUE(server.active_is(2));
    ASSERT_TRUE(server.online_is(2));

    winner.send("HSTA 200 leaving\n");
    EXPECT_EQ(observer.line(), "IQUI " + winner_sid + '\n');
    ASSERT_TRUE(server.active_is(1));
    ASSERT_TRUE(server.online_is(1));
    const auto marker = "BMSG " + observer_sid + " still-present\n";
    observer.send(marker);
    EXPECT_EQ(observer.line(), marker);
}

TEST(AdcPresence, RejectsRosterOverflowWithoutAnnouncingTheJoin) {
    const TestCertificate certificate;
    auto config = loopback_config(certificate);
    config.limits.pending_bytes = 1000;
    RunningServer server(config);
    RawClient member;
    RawClient newcomer;
    ASSERT_FALSE(member.connect(server.adc()));
    ASSERT_FALSE(newcomer.connect(server.adc()));
    const auto member_sid = negotiate(member);
    const auto newcomer_sid = negotiate(newcomer);
    const std::string description = " DE" + std::string(680, 'x') + '\n';
    auto wire = identification(member_sid);
    wire.pop_back();
    member.send(wire + description);
    expect_info(member, member_sid);

    wire = identification(newcomer_sid, second_cid, second_pid);
    wire.pop_back();
    newcomer.send(wire + description);
    std::string bytes;
    EXPECT_TRUE(socket_closed(newcomer.read_to_close(bytes)));
    EXPECT_TRUE(bytes.empty());
    EXPECT_TRUE(server.logged("Pending output limit exceeded"));
    ASSERT_TRUE(server.active_is(1));
    ASSERT_TRUE(server.online_is(1));
    const auto marker = "BMSG " + member_sid + " still-present\n";
    member.send(marker);
    EXPECT_EQ(member.line(), marker);

    RawClient replacement;
    ASSERT_FALSE(replacement.connect(server.adc()));
    const auto replacement_sid = negotiate(replacement);
    replacement.send(identification(replacement_sid, second_cid, second_pid, newcomer_sid));
    expect_info(replacement, member_sid);
    expect_info(replacement, replacement_sid, second_cid, newcomer_sid);
    expect_info(member, replacement_sid, second_cid, newcomer_sid);
    EXPECT_TRUE(server.online_is(2));
}

TEST(AdcPresence, RejectsPrivateEchoWhenTheRecipientQueueOverflows) {
    const TestCertificate certificate;
    auto config = loopback_config(certificate);
    config.limits.pending_bytes = 512;
    RunningServer server(config);
    RawClient sender;
    RawClient target;
    ASSERT_FALSE(sender.connect(server.adc()));
    ASSERT_FALSE(target.connect(server.adc()));
    const auto sender_sid = negotiate(sender);
    const auto target_sid = negotiate(target);
    identify(sender, sender_sid);
    target.send(identification(target_sid, second_cid, second_pid));
    expect_info(target, sender_sid);
    expect_info(target, target_sid, second_cid);
    expect_info(sender, target_sid, second_cid);

    sender.send("EMSG " + sender_sid + ' ' + target_sid + ' ' + std::string(600, 'x') + " PM" +
                sender_sid + '\n');
    EXPECT_EQ(sender.line(), "ISTA 125 Recipient\\sis\\sunavailable FCEMSG\n");
    EXPECT_EQ(sender.line(), "IQUI " + target_sid + '\n');
    std::string discarded;
    EXPECT_TRUE(socket_closed(target.read_to_close(discarded)));
    EXPECT_TRUE(discarded.empty());
    ASSERT_TRUE(server.active_is(1));
    ASSERT_TRUE(server.online_is(1));
    const auto marker = "BMSG " + sender_sid + " still-present\n";
    sender.send(marker);
    EXPECT_EQ(sender.line(), marker);
}

TEST(AdcPeerRouting, FindsFilesAndNegotiatesADirectSocketAcrossListeners) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    RawClient active;
    TlsClient passive(certificate.cert_path);
    ASSERT_FALSE(active.connect(server.adc()));
    ASSERT_FALSE(passive.connect(server.adcs()));
    ASSERT_FALSE(passive.handshake());
    const auto active_sid = negotiate(active);
    const auto passive_sid = negotiate(passive);
    active.send(identification(active_sid, adc::test::cid, adc::test::pid, {}, "BASE,TCP4"));
    expect_info(active, active_sid);
    passive.send(identification(passive_sid, second_cid, second_pid));
    expect_info(passive, active_sid);
    expect_info(passive, passive_sid, second_cid);
    expect_info(active, passive_sid, second_cid);

    const auto filtered = "FSCH " + passive_sid + " +TCP4 ANlocal ANfile TOfirst\n";
    const auto targeted = "DSCH " + passive_sid + ' ' + active_sid + " ANanother TOsecond\n";
    passive.send(filtered + targeted);
    EXPECT_EQ(active.line(), filtered);
    EXPECT_EQ(active.line(), targeted);
    const auto result = "DRES " + active_sid + ' ' + passive_sid +
                        " FN/Public/local\\sfile.txt SI37 SL2 TR" + std::string(adc::test::cid);
    const auto first_result = result + " TOfirst\n";
    const auto second_result = result + " TOsecond\n";
    active.send(second_result + first_result);
    EXPECT_EQ(passive.line(), second_result);
    EXPECT_EQ(passive.line(), first_result);

    asio::io_context peer_io;
    test::tcp::acceptor peer_listener(peer_io, {asio::ip::address_v4::loopback(), 0});
    const auto port = peer_listener.local_endpoint().port();
    const auto reverse = "DRCM " + passive_sid + ' ' + active_sid + " ADC/1.0 connect-token\n";
    passive.send(reverse);
    EXPECT_EQ(active.line(), reverse);
    const auto connect = "DCTM " + active_sid + ' ' + passive_sid + " ADC/1.0 " +
                         std::to_string(port) + " connect-token\n";
    active.send(connect);
    const auto forwarded = passive.line();
    ASSERT_EQ(forwarded, connect);
    const auto signal = adc::test::decode(forwarded);
    const auto request = adc::parse_peer_connection(signal);
    ASSERT_TRUE(request && request->port);

    RawClient peer;
    ASSERT_FALSE(peer.connect({"127.0.0.1", *request->port}));
    auto remote = peer_listener.accept();
    const auto handshake = "CSUP ADBASE ADTIGR\nCINF ID" + std::string(second_cid) + " TO" +
                           std::string(request->token) + '\n';
    peer.send(handshake);
    std::string wire(handshake.size(), '\0');
    asio::read(remote, asio::buffer(wire));
    EXPECT_EQ(wire, handshake);
    EXPECT_TRUE(server.online_is(2));
}

TEST(AdcPeerRouting, RejectsSpoofingMalformedSignalsAndFilesOnTheHub) {
    const TestCertificate certificate;
    RunningServer server(loopback_config(certificate));
    RawClient sender;
    RawClient peer;
    ASSERT_FALSE(sender.connect(server.adc()));
    ASSERT_FALSE(peer.connect(server.adc()));
    const auto sender_sid = negotiate(sender);
    const auto peer_sid = negotiate(peer);
    sender.send("BSCH " + sender_sid + " ANpremature\n");
    EXPECT_TRUE(sender.line().starts_with("ISTA 144 "));
    sender.send(identification(sender_sid, adc::test::cid, adc::test::pid, {}, "BASE,TCP4"));
    expect_info(sender, sender_sid);
    peer.send(identification(peer_sid, second_cid, second_pid));
    expect_info(peer, sender_sid);
    expect_info(peer, peer_sid, second_cid);
    expect_info(sender, peer_sid, second_cid);

    const auto marker = "BMSG " + sender_sid + " barrier\n";
    sender.send("BSCH " + peer_sid + " ANspoof\n" + "BSCH " + sender_sid +
                " ANmalformed TOduplicate TOanother\n" + "DRES " + sender_sid + ' ' + peer_sid +
                " FN/Public/x/ SI8 TOunsolicited\n" + "DCTM " + sender_sid + ' ' + peer_sid +
                " ADC/1.0 65536 invalid-port\n" + "CGET file fake 0 -1\n" + "CSND file fake 0 4\n" +
                marker);
    EXPECT_TRUE(sender.line().starts_with("ISTA 125 "));
    for (int index = 0; index < 3; ++index) {
        EXPECT_TRUE(sender.line().starts_with("ISTA 140 "));
    }
    EXPECT_TRUE(sender.line().starts_with("ISTA 125 "));
    EXPECT_TRUE(sender.line().starts_with("ISTA 125 "));
    EXPECT_EQ(sender.line(), marker);
    // The next frame at the intended target makes unintended forwarding observable.
    EXPECT_EQ(peer.line(), marker);
    peer.reset();
    EXPECT_EQ(sender.line(), "IQUI " + peer_sid + '\n');
    sender.send("DCTM " + sender_sid + ' ' + peer_sid + " ADC/1.0 8001 offline\n");
    EXPECT_EQ(sender.line(), "ISTA 125 Recipient\\sis\\sunavailable FCDCTM\n");
    EXPECT_TRUE(server.online_is(1));
}

} // namespace
} // namespace dc24h::net
