#include "test_certificate.hpp"
#include "test_client.hpp"

#include "../adc/test_messages.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <mutex>
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
                           std::string_view pid = adc::test::pid) {
    std::string wire = "BINF ";
    wire += sid;
    wire += " ID";
    wire += cid;
    wire += " PD";
    wire += pid;
    wire += " NIGuest SUBASE I4192.0.2.9\n";
    return wire;
}

template <typename Client>
void identify(Client &client, std::string_view sid, std::string_view cid = adc::test::cid,
              std::string_view pid = adc::test::pid) {
    client.send(identification(sid, cid, pid));
    const auto info = adc::test::decode(client.line());
    EXPECT_EQ(info.fourcc(), "BINF");
    EXPECT_EQ(info.sender_sid, sid);
    const auto fields = adc::parse_info(info);
    ASSERT_TRUE(fields);
    EXPECT_TRUE(
        std::ranges::none_of(*fields, [](const adc::Field &item) { return item.name == "PD"; }));
    EXPECT_TRUE(std::ranges::any_of(
        *fields, [cid](const adc::Field &item) { return item.name == "ID" && item.value == cid; }));
    EXPECT_TRUE(std::ranges::any_of(*fields, [](const adc::Field &item) {
        return item.name == "I4" && item.value == "127.0.0.1";
    }));
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
    identify(second, second_sid, second_cid, second_pid);
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
    identify(second, second_sid, second_cid, second_pid);
    EXPECT_FALSE(first.shutdown_tls());
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
    for (int index = 0; index < 120; ++index) {
        wire += ' ';
        wire += static_cast<char>('A' + index / 26);
        wire += static_cast<char>('A' + index % 26);
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
    identify(encrypted, adcs_sid, second_cid, second_pid);
    server.stop();
    server.stop();
    EXPECT_EQ(plain.line(), "IQUI " + adc_sid + '\n');
    EXPECT_EQ(encrypted.line(), "IQUI " + adcs_sid + '\n');
    EXPECT_FALSE(encrypted.shutdown_tls());
    server.join();
    EXPECT_TRUE(server.active_is(0));
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

} // namespace
} // namespace dc24h::net
