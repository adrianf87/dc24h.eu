#include "dc24h/adc/session.hpp"
#include "test_messages.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace dc24h::adc {
namespace {

void expect_status(const SessionResult &result, Severity severity, unsigned code,
                   std::string_view field_name = {}, std::string_view field_value = {}) {
    ASSERT_EQ(result.replies.size(), 1U);
    const auto message = test::decode(result.replies.front());
    EXPECT_EQ(message.type, MessageType::information);
    const auto parsed = parse_status(message);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->severity, severity);
    EXPECT_EQ(parsed->code, code);
    if (!field_name.empty()) {
        ASSERT_EQ(parsed->fields.size(), 1U);
        EXPECT_EQ(parsed->fields.front().name, field_name);
        EXPECT_EQ(parsed->fields.front().value, field_value);
    }
}

class AdcSession : public testing::Test {
  protected:
    Session session{"ABCD"};

    SessionResult receive(std::string_view wire) { return session.receive(test::decode(wire)); }

    void negotiate() {
        const auto result = receive("HSUP ADBASE ADTIGR ADFUTR\n");
        EXPECT_EQ(result.replies,
                  (std::vector<std::string>{"ISUP ADTIGR ADBASE\n", "ISID ABCD\n"}));
        EXPECT_EQ(session.state(), SessionState::identify);
    }

    void identify() {
        negotiate();
        const auto result = receive(test::identification());
        EXPECT_EQ(result.action, SessionAction::identified);
        EXPECT_TRUE(result.replies.empty());
        EXPECT_EQ(session.cid(), test::cid);
        EXPECT_EQ(session.state(), SessionState::identify);
    }
};

TEST_F(AdcSession, RejectsIllegalOrderAndServerOnlyCommandsFromClients) {
    EXPECT_THROW(static_cast<void>(Session("0000")), std::invalid_argument);
    EXPECT_FALSE(session.complete_identification());
    EXPECT_FALSE(session.complete_verification(true));
    expect_status(receive(test::identification()), Severity::recoverable, 44, "FC", "BINF");
    expect_status(receive("HPAS " + std::string(test::cid) + '\n'), Severity::recoverable, 44);
    expect_status(receive("ISID ABCD\n"), Severity::recoverable, 25, "FC", "ISID");
    expect_status(receive("HQUI ABCD\n"), Severity::recoverable, 25, "FC", "HQUI");
    expect_status(receive("IGPA " + std::string(test::pid) + '\n'), Severity::recoverable, 25);
    expect_status(receive("BMSG ABCD hello\n"), Severity::recoverable, 44);
    EXPECT_EQ(session.state(), SessionState::protocol);

    negotiate();
    EXPECT_FALSE(session.complete_identification());
    expect_status(receive("HSUP ADTCP4\n"), Severity::recoverable, 44);
    expect_status(receive("BMSG ABCD hello\n"), Severity::recoverable, 44);
    EXPECT_EQ(session.state(), SessionState::identify);
}

TEST_F(AdcSession, RequiresBaseAndACommonHashInInitialSupport) {
    expect_status(receive("HSUP XXBASE ADTIGR\n"), Severity::recoverable, 40);
    EXPECT_FALSE(session.closed());
    EXPECT_FALSE(session.peer_supports("TIGR"));
    const auto no_base = receive("HSUP ADTIGR ADFUTR\n");
    expect_status(no_base, Severity::fatal, 45, "FC", "BASE");
    EXPECT_EQ(no_base.action, SessionAction::close);
    EXPECT_TRUE(session.closed());
    EXPECT_EQ(session.state(), SessionState::protocol);
    EXPECT_TRUE(receive("HSUP ADBASE ADTIGR\n").replies.empty());

    Session other{"ABCD"};
    const auto no_hash = other.receive(test::decode("HSUP ADBASE ADFUTR\n"));
    expect_status(no_hash, Severity::fatal, 47);
    EXPECT_EQ(no_hash.action, SessionAction::close);
}

TEST_F(AdcSession, AcceptsGuestAndAppliesSupportUpdatesAtomically) {
    identify();
    EXPECT_TRUE(session.peer_supports("BASE"));
    EXPECT_TRUE(session.peer_supports("TIGR"));
    EXPECT_TRUE(session.peer_supports("FUTR"));
    ASSERT_TRUE(session.complete_identification());
    EXPECT_EQ(session.state(), SessionState::normal);
    EXPECT_FALSE(session.complete_identification());
    EXPECT_FALSE(session.complete_verification(true));

    const auto update = receive("HSUP RMFUTR ADTCP4 ADTCP4 RMXXXX\n");
    EXPECT_TRUE(update.replies.empty());
    EXPECT_FALSE(session.peer_supports("FUTR"));
    EXPECT_TRUE(session.peer_supports("TCP4"));
    expect_status(receive("HSUP RMBASE ADFUTR\n"), Severity::recoverable, 45, "FC", "BASE");
    EXPECT_TRUE(session.peer_supports("BASE"));
    EXPECT_FALSE(session.peer_supports("FUTR"));
    EXPECT_TRUE(receive("HSUP RMTIGR\n").replies.empty());
    EXPECT_FALSE(session.peer_supports("TIGR"));
    EXPECT_EQ(session.state(), SessionState::normal);
}

TEST_F(AdcSession, BoundsFeatureStateAcrossMultipleFrames) {
    identify();
    ASSERT_TRUE(session.complete_identification());
    std::string update = "HSUP";
    for (int index = 0; index < 61; ++index) {
        update += " ADZZ";
        update += static_cast<char>('A' + index / 26);
        update += static_cast<char>('A' + index % 26);
    }
    update += '\n';
    EXPECT_TRUE(receive(update).replies.empty());
    EXPECT_TRUE(session.peer_supports("ZZAA"));
    expect_status(receive("HSUP RMZZAA ADNEW1 ADNEW2\n"), Severity::recoverable, 40);
    EXPECT_TRUE(session.peer_supports("ZZAA"));
    EXPECT_FALSE(session.peer_supports("NEW1"));
}

TEST_F(AdcSession, RejectsMissingMalformedAndMismatchedIdentity) {
    negotiate();
    expect_status(receive("BINF ABCD NIGuest\n"), Severity::recoverable, 43, "FM", "ID");
    expect_status(receive("BINF ABCD ID" + std::string(test::cid) + '\n'), Severity::recoverable,
                  43, "FM", "PD");
    expect_status(receive(test::identification("bad")), Severity::recoverable, 43, "FB", "ID");
    expect_status(receive(test::identification(test::cid, "bad")), Severity::recoverable, 27);
    expect_status(receive(test::identification(test::pid)), Severity::recoverable, 27);
    EXPECT_EQ(session.state(), SessionState::identify);
    EXPECT_TRUE(session.cid().empty());
    EXPECT_FALSE(session.complete_identification());
    EXPECT_EQ(receive(test::identification()).action, SessionAction::identified);
    expect_status(receive(test::identification()), Severity::recoverable, 44);
}

TEST_F(AdcSession, GuardsDispatchAndNeverDispatchesThePrivateId) {
    identify();
    ASSERT_TRUE(session.complete_identification());
    expect_status(receive("BINF BAAD NIAttacker\n"), Severity::recoverable, 25);
    expect_status(receive(test::identification()), Severity::recoverable, 43, "FB", "PD");
    expect_status(receive("BINF ABCD ID" + std::string(test::pid) + '\n'), Severity::recoverable,
                  43, "FB", "ID");
    EXPECT_EQ(receive("BINF ABCD DE\n").action, SessionAction::dispatch);
    EXPECT_EQ(receive("DZZZ ABCD ZYX2 future\n").action, SessionAction::dispatch);
    EXPECT_EQ(receive("HZZZ future\n").action, SessionAction::none);
    EXPECT_EQ(receive("DSTA ABCD ZYX2 200 Error\n").action, SessionAction::dispatch);
    EXPECT_EQ(receive("DSTA BAAD ZYX2 200 Error\n").action, SessionAction::none);
    EXPECT_FALSE(session.closed());
}

TEST_F(AdcSession, VerificationRequiresAValidResponseAndCallerApproval) {
    identify();
    EXPECT_FALSE(session.complete_identification("AA"));
    EXPECT_EQ(session.state(), SessionState::identify);
    const auto result = session.complete_identification(test::pid);
    ASSERT_TRUE(result);
    ASSERT_EQ(result->replies.size(), 1U);
    const auto challenge = test::decode(result->replies.front());
    EXPECT_EQ(challenge.fourcc(), "IGPA");
    EXPECT_EQ(parse_password_challenge(challenge), test::pid);
    EXPECT_EQ(session.state(), SessionState::verify);
    EXPECT_FALSE(session.complete_verification(true));
    expect_status(receive("HSUP ADTCP4\n"), Severity::recoverable, 44);
    expect_status(receive(test::identification()), Severity::recoverable, 44);
    expect_status(receive("HPAS AA\n"), Severity::recoverable, 40);
    EXPECT_FALSE(session.complete_verification(true));

    const auto response = "HPAS " + std::string(test::cid) + '\n';
    EXPECT_EQ(receive(response).action, SessionAction::verify_password);
    EXPECT_EQ(session.state(), SessionState::verify);
    expect_status(receive(response), Severity::recoverable, 44);
    ASSERT_TRUE(session.complete_verification(true));
    EXPECT_EQ(session.state(), SessionState::normal);
    EXPECT_FALSE(session.complete_verification(true));
    expect_status(receive(response), Severity::recoverable, 44);
}

TEST_F(AdcSession, RejectedVerificationClosesWithoutAdvancingState) {
    identify();
    ASSERT_TRUE(session.complete_identification(test::pid));
    EXPECT_EQ(receive("HPAS " + std::string(test::cid) + '\n').action,
              SessionAction::verify_password);
    const auto rejected = session.complete_verification(false);
    ASSERT_TRUE(rejected);
    expect_status(*rejected, Severity::fatal, 23);
    EXPECT_EQ(rejected->action, SessionAction::close);
    EXPECT_TRUE(session.closed());
    EXPECT_FALSE(session.complete_verification(true));
    EXPECT_EQ(session.state(), SessionState::verify);
}

TEST_F(AdcSession, AcceptsClientStatusInEveryStateAndClosesOnFatalStatus) {
    EXPECT_TRUE(receive("HSTA 000 Okay\n").replies.empty());
    EXPECT_EQ(session.state(), SessionState::protocol);
    identify();
    EXPECT_TRUE(receive("HSTA 140 Retry\n").replies.empty());
    EXPECT_EQ(session.state(), SessionState::identify);
    ASSERT_TRUE(session.complete_identification(test::pid));
    EXPECT_TRUE(receive("HSTA 140 Retry\n").replies.empty());
    EXPECT_EQ(session.state(), SessionState::verify);
    EXPECT_EQ(receive("HPAS " + std::string(test::cid) + '\n').action,
              SessionAction::verify_password);
    ASSERT_TRUE(session.complete_verification(true));
    EXPECT_TRUE(receive("HSTA 000 Okay\n").replies.empty());
    EXPECT_TRUE(receive("HSTA 399 Wrong\n").replies.empty());
    EXPECT_EQ(session.state(), SessionState::normal);
    EXPECT_FALSE(session.closed());

    const auto fatal = receive("HSTA 299 Goodbye\n");
    EXPECT_EQ(fatal.action, SessionAction::close);
    EXPECT_TRUE(fatal.replies.empty());
    EXPECT_TRUE(session.closed());
    EXPECT_TRUE(session.disconnect().replies.empty());
}

TEST_F(AdcSession, SendsQuitOnlyAfterSidAssignmentAndOnlyOnce) {
    Session fresh("ABCD");
    expect_status(fresh.disconnect(), Severity::fatal, 0);
    identify();
    const auto quit = session.disconnect();
    EXPECT_TRUE(session.closed());
    EXPECT_EQ(quit.action, SessionAction::close);
    ASSERT_EQ(quit.replies.size(), 1U);
    const auto message = test::decode(quit.replies.front());
    EXPECT_EQ(message.fourcc(), "IQUI");
    ASSERT_TRUE(parse_quit(message));
    EXPECT_EQ(parse_quit(message)->sid, "ABCD");
    EXPECT_TRUE(session.disconnect().replies.empty());
}

TEST(AdcIntegration, HandlesFragmentationMultipleFramesAndInvalidInputInOneStream) {
    const std::string first = "BINF ABCD IDbad\nHS";
    const std::string second = "UP ADBASE ADTIGR\nHMSG bad\\x\n" + test::identification();
    FrameDecoder decoder;
    Session session("ABCD");
    std::vector<std::string> replies;
    unsigned parse_errors = 0;
    for (std::string_view chunk : {std::string_view(first), std::string_view(second)}) {
        while (!chunk.empty()) {
            const auto decoded = decoder.consume(chunk);
            ASSERT_GT(decoded.consumed, 0U);
            chunk.remove_prefix(decoded.consumed);
            if (!decoded.frame) {
                continue;
            }
            if (std::holds_alternative<ParseError>(*decoded.frame)) {
                ++parse_errors;
                continue;
            }
            const auto result = session.receive(std::get<Message>(*decoded.frame));
            replies.insert(replies.end(), result.replies.begin(), result.replies.end());
            if (result.action == SessionAction::identified) {
                ASSERT_TRUE(session.complete_identification());
            }
        }
    }
    ASSERT_EQ(replies.size(), 3U);
    EXPECT_EQ(test::decode(replies[0]).fourcc(), "ISTA");
    EXPECT_EQ(replies[1], "ISUP ADTIGR ADBASE\n");
    EXPECT_EQ(replies[2], "ISID ABCD\n");
    EXPECT_EQ(parse_errors, 1U);
    EXPECT_FALSE(decoder.finish());
    EXPECT_EQ(session.state(), SessionState::normal);
    EXPECT_EQ(session.cid(), test::cid);
}

} // namespace
} // namespace dc24h::adc
