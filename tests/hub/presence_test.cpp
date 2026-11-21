#include "dc24h/hub/presence.hpp"

#include "../adc/test_messages.hpp"

#include <gtest/gtest.h>

namespace dc24h::hub {
namespace {

using adc::test::decode;
using namespace std::chrono_literals;

constexpr auto now = Presence::Clock::time_point{};
constexpr std::string_view second_cid = "ZXO4VT7KPNYLJBLFLOR5YP3A33SPNOHYMEDJ4MY";

adc::Message profile(std::string_view sid, std::string_view nickname,
                     std::string_view cid = adc::test::cid, std::string_view extra = {}) {
    std::string wire = "BINF ";
    wire += sid;
    wire += " ID";
    wire += cid;
    wire += " NI";
    wire += adc::escape_parameter(nickname);
    wire += extra;
    wire += '\n';
    return decode(wire);
}

class UserPresence : public testing::Test {
  protected:
    Presence users{32, 32, 256};
};

TEST_F(UserPresence, AdmitsUniqueSessionsAndReusesNamesAfterOneDeparture) {
    const auto first = users.admit("ABCD", profile("ABCD", "Straße"), now);
    ASSERT_TRUE(std::holds_alternative<WireFrame>(first));
    EXPECT_EQ(users.find("ABCD"), std::get<WireFrame>(first));
    EXPECT_FALSE(users.find("ABCE"));
    EXPECT_EQ(users.roster().size(), 1U);
    EXPECT_EQ(
        std::get<Rejection>(users.admit("ABCD", profile("ABCD", "New", second_cid), now)).code,
        "25");
    EXPECT_EQ(
        std::get<Rejection>(users.admit("ABCE", profile("ABCE", "STRASSE", second_cid), now)).code,
        "22");
    EXPECT_EQ(std::get<Rejection>(users.admit("ABCE", profile("ABCE", "New"), now)).code, "24");
    EXPECT_TRUE(users.remove("ABCD"));
    EXPECT_FALSE(users.remove("ABCD"));
    EXPECT_TRUE(users.roster().empty());

    ASSERT_TRUE(
        std::holds_alternative<WireFrame>(users.admit("ABCE", profile("ABCE", "STRASSE"), now)));
    EXPECT_EQ(users.roster()[0].sid, "ABCE");
    EXPECT_FALSE(users.find("ABCD"));
}

TEST_F(UserPresence, ValidatesNicknameCodePointsAndNfc) {
    Presence short_names{2, 32, 256};
    for (const auto nickname : {"", "a b", "a\u00a0", "a\u200d", "a\x7f", "Żół"}) {
        EXPECT_TRUE(std::holds_alternative<Rejection>(
            short_names.admit("ABCD", profile("ABCD", nickname), now)))
            << nickname;
    }
    EXPECT_TRUE(
        std::holds_alternative<WireFrame>(short_names.admit("ABCD", profile("ABCD", "Żó"), now)));
    EXPECT_EQ(std::get<adc::ParseError>(adc::parse_frame("BINF ABCD NIa\u0301\n")),
              adc::ParseError::non_nfc);
}

TEST_F(UserPresence, UpdatesPublicAttributesAtomicallyAndClearsOptionalFields) {
    const auto first = users.admit(
        "ABCD", profile("ABCD", "Alpha", adc::test::cid, " DEone PDprivate EMhidden CT4 ZZunknown"),
        now);
    ASSERT_TRUE(std::holds_alternative<WireFrame>(first));
    ASSERT_TRUE(std::holds_alternative<WireFrame>(
        users.admit("ABCE", profile("ABCE", "Beta", second_cid), now)));
    EXPECT_EQ(*users.find("ABCD"),
              "BINF ABCD DEone ID" + std::string(adc::test::cid) + " NIAlpha\n");

    const auto denied = users.update("ABCD", decode("BINF ABCD NIbETA DEpending\n"), now);
    EXPECT_EQ(std::get<Rejection>(denied).code, "22");
    EXPECT_EQ(users.find("ABCD"), std::get<WireFrame>(first));
    const auto changed =
        users.update("ABCD", decode("BINF ABCD NIGamma DE SUUDP4 EMhidden\n"), now);
    EXPECT_EQ(*std::get<WireFrame>(changed), "BINF ABCD DE NIGamma SUUDP4\n");
    EXPECT_EQ(*users.find("ABCD"),
              "BINF ABCD ID" + std::string(adc::test::cid) + " NIGamma SUUDP4\n");
    EXPECT_FALSE(std::get<WireFrame>(users.update("ABCD", decode("BINF ABCD NIGamma\n"), now)));
    EXPECT_TRUE(
        std::holds_alternative<Rejection>(users.update("ABCD", decode("BINF ABCD NI\n"), now)));
    EXPECT_TRUE(std::holds_alternative<Rejection>(
        users.update("ABCD", decode("BINF ABCD ID" + std::string(second_cid) + '\n'), now)));
    EXPECT_EQ(std::get<Rejection>(users.update("ABCD", decode("BINF ABCE NIForged\n"), now)).code,
              "25");
}

TEST_F(UserPresence, BoundsStoredProfileAndChecksNamedFields) {
    const auto first = users.admit("ABCD", profile("ABCD", "Alpha"), now);
    ASSERT_TRUE(std::holds_alternative<WireFrame>(first));
    for (const auto &malformed : {"U465536", "SUUDP4,UDP4", "SUBASE,", "AW3", "SS-1", "NIx NIy"}) {
        EXPECT_TRUE(std::holds_alternative<Rejection>(
            users.update("ABCD", decode(std::string("BINF ABCD ") + malformed + '\n'), now)))
            << malformed;
    }
    EXPECT_EQ(users.find("ABCD"), std::get<WireFrame>(first));
    const auto first_large =
        users.update("ABCD", decode("BINF ABCD DE" + std::string(140, 'x') + '\n'), now);
    ASSERT_TRUE(std::holds_alternative<WireFrame>(first_large));
    const auto stored = users.find("ABCD");
    EXPECT_TRUE(std::holds_alternative<Rejection>(
        users.update("ABCD", decode("BINF ABCD VE" + std::string(140, 'x') + '\n'), now)));
    EXPECT_EQ(users.find("ABCD"), stored);
}

TEST_F(UserPresence, RoutesBroadcastsFeaturesAndPrivateMessages) {
    ASSERT_TRUE(
        std::holds_alternative<WireFrame>(users.admit("ABCD", profile("ABCD", "Alpha"), now)));
    ASSERT_TRUE(std::holds_alternative<WireFrame>(
        users.admit("ABCE", profile("ABCE", "Beta", second_cid, " SUUDP4"), now)));
    const auto chat = [&](std::string_view wire) {
        return std::get<ChatDelivery>(users.chat("ABCD", decode(wire), now));
    };
    const std::string public_chat = "BMSG ABCD Zażółć\\sgęślą\\n ME1\n";
    const auto broadcast = chat(public_chat);
    EXPECT_EQ(*broadcast.frame, public_chat);
    EXPECT_EQ(broadcast.recipients, (std::vector<std::string_view>{"ABCD", "ABCE"}));

    const auto feature = chat("FMSG ABCD +UDP4-UDP6 test\n");
    EXPECT_EQ(feature.recipients, (std::vector<std::string_view>{"ABCE"}));
    const auto direct = chat("DMSG ABCD ABCE private PMABCD\n");
    EXPECT_EQ(*direct.frame, "DMSG ABCD ABCE private PMABCD\n");
    EXPECT_EQ(direct.recipients, (std::vector<std::string_view>{"ABCE"}));
    const auto echo = chat("EMSG ABCD ABCE private PMABCD\n");
    EXPECT_EQ(echo.recipients, (std::vector<std::string_view>{"ABCE", "ABCD"}));
    const auto self = chat("EMSG ABCD ABCD private PMABCD\n");
    EXPECT_EQ(self.recipients, (std::vector<std::string_view>{"ABCD"}));
}

TEST_F(UserPresence, RejectsMisroutedMessagesAndDoesNotEchoPrivateData) {
    ASSERT_TRUE(
        std::holds_alternative<WireFrame>(users.admit("ABCD", profile("ABCD", "Alpha"), now)));
    EXPECT_EQ(std::get<Rejection>(users.chat("ABCD", decode("BMSG ABCE spoofed\n"), now)).code,
              "25");
    EXPECT_EQ(std::get<Rejection>(users.chat("ABCE", decode("BMSG ABCE premature\n"), now)).code,
              "25");
    const auto unavailable = users.chat("ABCD", decode("EMSG ABCD ABCF secret PMABCD\n"), now);
    EXPECT_EQ(*std::get<Rejection>(unavailable).reply(),
              "ISTA 125 Recipient\\sis\\sunavailable FCEMSG\n");
    for (const auto wire :
         {"BMSG ABCD secret PMABCD\n", "FMSG ABCD +BASE secret PMABCD\n",
          "DMSG ABCD ABCD secret PMZZZZ\n", "DMSG ABCD ABCD secret\n", "BMSG ABCD secret ME1 ME0\n",
          "BMSG ABCD secret ME2\n", "BMSG ABCD\n"}) {
        EXPECT_EQ(std::get<Rejection>(users.chat("ABCD", decode(wire), now)).code, "40") << wire;
    }
}

TEST_F(UserPresence, LimitsDecodedBytesAndSharesRateBudgetWithInfUpdates) {
    Presence limited{32, 4, 256};
    ASSERT_TRUE(
        std::holds_alternative<WireFrame>(limited.admit("ABCD", profile("ABCD", "Alpha"), now)));
    EXPECT_TRUE(
        std::holds_alternative<Rejection>(limited.chat("ABCD", decode("BMSG ABCD ééé\n"), now)));
    EXPECT_TRUE(std::holds_alternative<ChatDelivery>(
        limited.chat("ABCD", decode("BMSG ABCD é\\sa\n"), now)));
    for (int n = 0; n < 6; ++n) {
        EXPECT_TRUE(std::holds_alternative<WireFrame>(
            limited.update("ABCD", decode("BINF ABCD DEtest\n"), now)));
    }
    const auto chat = decode("BMSG ABCD test\n");
    EXPECT_EQ(std::get<Rejection>(limited.chat("ABCD", chat, now + 249ms)).description,
              "Update or chat rate limit exceeded");
    EXPECT_TRUE(std::holds_alternative<ChatDelivery>(limited.chat("ABCD", chat, now + 250ms)));
    EXPECT_TRUE(std::holds_alternative<Rejection>(limited.chat("ABCD", chat, now + 250ms)));
}

} // namespace
} // namespace dc24h::hub
