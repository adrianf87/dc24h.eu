#include "dc24h/hub/peer_routing.hpp"

#include "../adc/test_messages.hpp"

#include <gtest/gtest.h>

namespace dc24h::hub {
namespace {

using adc::test::decode;
using namespace std::chrono_literals;

constexpr auto start = PeerRouting::Clock::time_point{};
constexpr std::string_view passive = "ABCD";
constexpr std::string_view active = "ABCE";
constexpr std::string_view other = "ABCF";
constexpr std::string_view active_cid = "ZXO4VT7KPNYLJBLFLOR5YP3A33SPNOHYMEDJ4MY";
constexpr std::string_view other_cid = "2OAQNIXGXYDKKV5VMUWQJMEYEFQ64QPULDCLPTI";
constexpr std::string_view active_fields = " I4127.0.0.1 U42123 SUBASE,TCP4,UDP4,ADCS,ADC0";

std::string result(std::string_view from, std::string_view to, std::string_view token = {}) {
    std::string wire = "DRES ";
    wire += from;
    wire += ' ';
    wire += to;
    wire += " FN/Public/track.ogg SI7 SL1 TR";
    wire += adc::test::cid;
    if (!token.empty()) {
        wire += " TO";
        wire += token;
    }
    wire += '\n';
    return wire;
}

class PeerSignaling : public testing::Test {
  protected:
    void SetUp() override {
        admit(passive, adc::test::cid, " SUBASE,ADCS");
        admit(active, active_cid, active_fields);
        // A feature claim without a verified address does not make a client TCP-active.
        admit(other, other_cid, " SUBASE,TCP6");
    }

    void admit(std::string_view sid, std::string_view cid, std::string_view fields) {
        std::string wire = "BINF ";
        wire += sid;
        wire += " NI";
        wire += sid;
        wire += " ID";
        wire += cid;
        wire += fields;
        wire += '\n';
        ASSERT_TRUE(std::holds_alternative<WireFrame>(users.admit(sid, decode(wire), start)));
    }

    std::optional<Rejection> route(std::string_view from, std::string_view wire,
                                   PeerRouting::Clock::time_point now = start) {
        return peers.route(from, decode(wire), now);
    }

    void depart(std::string_view sid) {
        ASSERT_TRUE(users.remove(sid));
        peers.forget(sid);
    }

    Presence users{32, 1024, 16384};
    std::vector<std::pair<std::string, std::string>> forwarded;
    std::string_view blocked;
    PeerRouting peers{users, [&](std::string_view sid, const WireFrame &frame) {
                          if (sid == blocked) {
                              return false;
                          }
                          forwarded.emplace_back(sid, *frame);
                          return true;
                      }};
};

TEST_F(PeerSignaling, FiltersSearchAndRequiresEligibleSenderAndIntendedRequester) {
    const std::string search = "FSCH ABCD +TCP4-TCP6 ANtrack TOroom\\sone\n";
    ASSERT_FALSE(route(passive, search));
    ASSERT_EQ(forwarded.size(), 1U);
    EXPECT_EQ(forwarded[0], std::make_pair(std::string(active), search));
    EXPECT_TRUE(route(other, result(other, passive, "room\\sone")));
    EXPECT_TRUE(route(active, result(active, other, "room\\sone")));
    EXPECT_TRUE(route(active, result(active, passive, "wrong")));
    EXPECT_TRUE(route(other, result(active, passive, "room\\sone")));
    EXPECT_EQ(forwarded.size(), 1U);
    const auto good = result(active, passive, "room\\sone");
    ASSERT_FALSE(route(active, good));
    ASSERT_EQ(forwarded.size(), 2U);
    EXPECT_EQ(forwarded.back(), std::make_pair(std::string(passive), good));
}

TEST_F(PeerSignaling, SupportsParallelTokensAndTokenlessClients) {
    ASSERT_FALSE(route(passive, "BSCH ABCD ANtrack\n"));
    ASSERT_FALSE(route(active, "BSCH ABCE ANnotes TOshared\n"));
    ASSERT_FALSE(route(passive, "DSCH ABCD ABCE ANalbum TOshared\n"));
    EXPECT_EQ(forwarded.size(), 7U);
    EXPECT_FALSE(route(active, result(active, passive)));
    EXPECT_FALSE(route(passive, result(passive, active, "shared")));
    EXPECT_FALSE(route(active, result(active, passive, "shared")));
    EXPECT_TRUE(route(other, result(other, passive, "shared")));
    EXPECT_TRUE(route(passive, result(passive, active)));
    ASSERT_EQ(forwarded.size(), 10U);
}

TEST_F(PeerSignaling, ExpiresEntriesAndDropsAuthorizationOnSidReuse) {
    ASSERT_FALSE(route(passive, "DSCH ABCD ABCE ANtrack TOone\n"));
    ASSERT_FALSE(route(passive, "DSCH ABCD ABCF ANtrack TOone\n", start + 35s));
    EXPECT_TRUE(route(active, result(active, passive, "one"), start + 60s));
    EXPECT_FALSE(route(other, result(other, passive, "one"), start + 60s));
    depart(other);
    admit(other, other_cid, " SUBASE");
    EXPECT_TRUE(route(other, result(other, passive, "one"), start + 61s));
    depart(passive);
    admit(passive, adc::test::cid, " SUBASE");
    EXPECT_TRUE(route(active, result(active, passive, "one"), start + 61s));
}

TEST_F(PeerSignaling, BoundsSearchRateAndResultCount) {
    for (int index = 0; index < 8; ++index) {
        const auto wire = "DSCH ABCD ABCE ANtrack TO" + std::to_string(index) + '\n';
        ASSERT_FALSE(route(passive, wire));
    }
    EXPECT_TRUE(route(passive, "DSCH ABCD ABCE ANtrack TOninth\n", start + 999ms));
    EXPECT_FALSE(route(passive, "DSCH ABCD ABCE ANtrack TOninth\n", start + 1s));
    for (int index = 0; index < 5; ++index) {
        EXPECT_FALSE(route(active, result(active, passive, "1"), start + 1s));
    }
    const auto denied = route(active, result(active, passive, "1"), start + 1s);
    ASSERT_TRUE(denied);
    EXPECT_EQ(denied->description, "Search result limit reached");
    EXPECT_FALSE(route(active, result(active, passive, "2"), start + 1s));
}

TEST_F(PeerSignaling, ChecksModeAndMatchesTheProtocolAndTokenForReverseConnections) {
    EXPECT_TRUE(route(passive, "DCTM ABCD ABCE ADC/1.0 8001 passive\n"));
    EXPECT_TRUE(route(other, "DCTM ABCF ABCE ADC/1.0 8001 no-ip\n"));
    EXPECT_TRUE(route(passive, "DRCM ABCD ABCF ADC/1.0 no-active-peer\n"));
    EXPECT_TRUE(route(active, "DCTM ABCE ABCF ADCS/1.0 8001 no-tls\n"));
    EXPECT_TRUE(forwarded.empty());

    const std::string request = "DRCM ABCD ABCE ADCS/1.0 pending\n";
    ASSERT_FALSE(route(passive, request));
    EXPECT_TRUE(route(active, "DCTM ABCE ABCD ADCS/1.0 8001 wrong\n"));
    EXPECT_TRUE(route(active, "DCTM ABCE ABCD ADC/1.0 8001 pending\n"));
    ASSERT_EQ(forwarded.size(), 1U);
    EXPECT_EQ(forwarded.back(), std::make_pair(std::string(active), request));
    const std::string response = "ECTM ABCE ABCD ADCS/1.0 8001 pending\n";
    ASSERT_FALSE(route(active, response));
    ASSERT_EQ(forwarded.size(), 3U);
    EXPECT_EQ(forwarded[1], std::make_pair(std::string(passive), response));
    EXPECT_EQ(forwarded[2], std::make_pair(std::string(active), response));
    EXPECT_FALSE(route(active, "DCTM ABCE ABCD ADC/1.0 8001 standalone\n"));
}

TEST_F(PeerSignaling, DoesNotAuthorizeUndeliveredRequestsAndUsesCurrentProfile) {
    blocked = active;
    EXPECT_TRUE(route(passive, "DSCH ABCD ABCE ANtrack TOundelivered\n"));
    EXPECT_TRUE(route(passive, "DRCM ABCD ABCE ADC/1.0 undelivered\n"));
    blocked = {};
    EXPECT_TRUE(route(active, result(active, passive, "undelivered")));
    EXPECT_FALSE(route(active, "DCTM ABCE ABCD ADC/1.0 8001 direct\n"));
    ASSERT_TRUE(std::holds_alternative<WireFrame>(
        users.update(active, decode("BINF ABCE SUADCS I4\n"), start)));
    EXPECT_TRUE(route(active, "DCTM ABCE ABCD ADC/1.0 8001 changed\n"));
    EXPECT_TRUE(route(passive, "DRCM ABCD 7777 ADC/1.0 offline\n"));
    EXPECT_TRUE(route(passive, "DRCM ABCD ABCE ADC/1.0 now-passive\n"));
    EXPECT_EQ(forwarded.size(), 1U);
}

} // namespace
} // namespace dc24h::hub
