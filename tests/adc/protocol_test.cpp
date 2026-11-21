#include "test_messages.hpp"

#include <gtest/gtest.h>

namespace dc24h::adc {
namespace {

TEST(AdcProtocol, ValidatesSidAndCanonicalTigerCid) {
    EXPECT_TRUE(is_sid("A27Z"));
    EXPECT_FALSE(is_sid("ABC"));
    EXPECT_FALSE(is_sid("ABC0"));
    EXPECT_FALSE(is_sid("aBCD"));
    EXPECT_TRUE(is_cid(test::cid));
    EXPECT_FALSE(is_cid("ABCD"));
    EXPECT_FALSE(is_cid(std::string(test::cid) + "="));
    EXPECT_FALSE(is_cid(std::string(test::cid.substr(0, 38)) + 'B'));
    EXPECT_FALSE(is_cid(std::string(test::cid.substr(0, 38)) + 'a'));

    EXPECT_EQ(verify_tiger_identity(test::cid, test::pid), IdentityCheck::valid);
    EXPECT_EQ(verify_tiger_identity(test::pid, test::pid), IdentityCheck::mismatch);
    EXPECT_EQ(verify_tiger_identity("ABCD", test::pid), IdentityCheck::invalid_cid);
    EXPECT_EQ(verify_tiger_identity(test::cid, "ABCD"), IdentityCheck::invalid_pid);
}

TEST(AdcProtocol, ParsesSupportChangesAndRejectsInvalidOperations) {
    const auto message = test::decode("HSUP ADBASE ADTIGR RMTCP4 ADFUTR\n");
    const auto changes = parse_support(message);
    ASSERT_TRUE(changes);
    ASSERT_EQ(changes->size(), 4U);
    EXPECT_EQ((*changes)[0].name, "BASE");
    EXPECT_EQ((*changes)[1].name, "TIGR");
    EXPECT_EQ((*changes)[2].name, "TCP4");
    EXPECT_FALSE((*changes)[2].added);
    EXPECT_EQ((*changes)[3].name, "FUTR");
    EXPECT_TRUE((*changes)[3].added);
    EXPECT_FALSE(parse_support(test::decode("HSUP\n")));
    EXPECT_FALSE(parse_support(test::decode("HSUP XXBASE\n")));
    EXPECT_FALSE(parse_support(test::decode("HSUP ADbase\n")));
    EXPECT_FALSE(parse_support(test::decode("HSUP ADbase1\n")));
    EXPECT_FALSE(parse_support(test::decode("HSUP ADT1G2\n")));
}

TEST(AdcProtocol, ParsesSidAndIncrementalInfFields) {
    const auto sid = test::decode("ISID ABCD\n");
    EXPECT_EQ(parse_sid(sid), "ABCD");
    EXPECT_FALSE(parse_sid(test::decode("ISID ABCD extra\n")));
    EXPECT_FALSE(parse_sid(test::decode("ISID 0000\n")));

    const auto message = test::decode("BINF ABCD NIJane\\sDoe DE I40.0.0.0\n");
    const auto fields = parse_info(message);
    ASSERT_TRUE(fields);
    ASSERT_EQ(fields->size(), 3U);
    EXPECT_EQ((*fields)[0].name, "NI");
    EXPECT_EQ((*fields)[0].value, "Jane Doe");
    EXPECT_EQ((*fields)[1].name, "DE");
    EXPECT_TRUE((*fields)[1].value.empty());
    EXPECT_EQ((*fields)[2].name, "I4");
    EXPECT_FALSE(parse_info(test::decode("BINF ABCD NIone NItwo\n")));
    EXPECT_FALSE(parse_info(test::decode("BINF ABCD N\n")));
}

TEST(AdcProtocol, ParsesStaSeverityCodeDescriptionAndFlags) {
    const auto failure = test::decode("ISTA 144 Invalid\\sstate FCHSUP\n");
    const auto status = parse_status(failure);
    ASSERT_TRUE(status);
    EXPECT_EQ(status->severity, Severity::recoverable);
    EXPECT_EQ(status->code, 44U);
    EXPECT_EQ(status->description, "Invalid state");
    ASSERT_EQ(status->fields.size(), 1U);
    EXPECT_EQ(status->fields.front().name, "FC");
    EXPECT_EQ(status->fields.front().value, "HSUP");

    const auto unknown = test::decode("HSTA 299 Extension\\smessage\n");
    ASSERT_TRUE(parse_status(unknown));
    EXPECT_EQ(parse_status(unknown)->code, 99U);
    EXPECT_EQ(parse_status(unknown)->severity, Severity::fatal);
    const auto success = test::decode("HSTA 000 OK\n");
    ASSERT_TRUE(parse_status(success));
    EXPECT_EQ(parse_status(success)->severity, Severity::success);
    for (const auto wire : {"HSTA 001 Wrong\n", "HSTA 301 Wrong\n", "HSTA 00A Wrong\n",
                            "HSTA 200\n", "HSTA 200 Wrong x\n"}) {
        EXPECT_FALSE(parse_status(test::decode(wire)));
    }
}

TEST(AdcProtocol, ParsesQuitAndPasswordMessages) {
    const auto quit_message = test::decode("IQUI ABCD IDZYX2 MSHub\\sclosed TL-1 DI\n");
    const auto quit = parse_quit(quit_message);
    ASSERT_TRUE(quit);
    EXPECT_EQ(quit->sid, "ABCD");
    ASSERT_EQ(quit->fields.size(), 4U);
    EXPECT_EQ(quit->fields[0].value, "ZYX2");
    EXPECT_EQ(quit->fields[1].value, "Hub closed");
    EXPECT_FALSE(parse_quit(test::decode("IQUI ABC1\n")));
    EXPECT_FALSE(parse_quit(test::decode("IQUI ABCD IDinvalid\n")));

    const auto challenge = test::decode("IGPA " + std::string(test::pid) + '\n');
    EXPECT_EQ(parse_password_challenge(challenge), test::pid);
    EXPECT_FALSE(parse_password_challenge(test::decode("IGPA MZXW6\n")));
    const auto response = test::decode("HPAS " + std::string(test::cid) + '\n');
    EXPECT_EQ(parse_password_response(response), test::cid);
    EXPECT_FALSE(parse_password_response(test::decode("HPAS MZXW6\n")));
}

TEST(AdcProtocol, AcceptsRepeatedSearchTermsAndChecksScalarFlags) {
    const std::string wire =
        "FSCH ABCD +TCP4 ANplik ANdo\\stestów NOold EXogg EXwav GE7 LE40 TY1 TOquery\\sone\n";
    const auto request = test::decode(wire);
    EXPECT_EQ(parse_search(request), "query one");
    EXPECT_EQ(format_routed(request), wire);
    EXPECT_EQ(parse_search(test::decode("BSCH ABCD ANlocal ZZfuture\n")), "");
    EXPECT_EQ(parse_search(test::decode("BSCH ABCD TR" + std::string(test::cid) + '\n')), "");
    for (const auto malformed : {"AN TOtoken", "EX.txt", "TRbad", "TY3", "GE10 LE2", "GE10 EQ5",
                                 "GE-1", "GE18446744073709551616", "ANfile TO", "ANfile TOa TOb",
                                 "ANfile TOa\\nb", "TOtoken ZZunknown"}) {
        EXPECT_FALSE(parse_search(test::decode(std::string("BSCH ABCD ") + malformed + '\n')))
            << malformed;
    }
}

TEST(AdcProtocol, ValidatesFileAndDirectorySearchResults) {
    const std::string file =
        "DRES ABCE ABCD FN/share/a\\sb.txt SI7 SL2 TR" + std::string(test::cid);
    EXPECT_EQ(parse_search_result(test::decode(file + " TOresult\\sone\n")), "result one");
    EXPECT_EQ(parse_search_result(test::decode(file + '\n')), "");
    EXPECT_EQ(parse_search_result(test::decode("DRES ABCE ABCD FN/share/subdir/ SI0\n")), "");
    for (const auto malformed : {"FN/share/test SI1 TOq", "FNrelative/ SI0", "FN/share/x/",
                                 "FN/share/x/ SI-1", "FN/share/x/ SI0 SI1", "FN/share/x/ SI0 TRxx",
                                 "FN/share/x/ SI0 SLnone", "FN/share/x/ SI0 TO"}) {
        EXPECT_FALSE(
            parse_search_result(test::decode(std::string("DRES ABCE ABCD ") + malformed + '\n')))
            << malformed;
    }
}

TEST(AdcProtocol, ValidatesPortsOpaqueTokensAndKeyprints) {
    const auto ctm = test::decode("DCTM ABCD ABCE ADCS/1.0 65535 local\\stoken\n");
    const auto connect = parse_peer_connection(ctm);
    ASSERT_TRUE(connect);
    EXPECT_EQ(connect->protocol, "ADCS/1.0");
    EXPECT_EQ(connect->port, 65535);
    EXPECT_EQ(connect->token, "local token");
    const auto rcm = test::decode("DRCM ABCD ABCE ADC/1.0 reverse\n");
    const auto reverse = parse_peer_connection(rcm);
    ASSERT_TRUE(reverse);
    EXPECT_FALSE(reverse->port);
    EXPECT_EQ(reverse->token, "reverse");
    for (const auto malformed :
         {"ADC/1.0 0 tok", "ADC/1.0 +1 tok", "ADC/1.0 65536 tok", "ADC/1.0 12.3 tok", "ADC/1.0 9",
          "ADC/1.0 9 tok extra", "ADC/1.0 9 tok\\nnext"}) {
        EXPECT_FALSE(
            parse_peer_connection(test::decode(std::string("DCTM ABCD ABCE ") + malformed + '\n')))
            << malformed;
    }
    EXPECT_FALSE(parse_peer_connection(
        test::decode("DRCM ABCD ABCE ADC/1.0 " + std::string(257, 'x') + '\n')));
    const auto key = "SHA256/" + std::string(52, 'A');
    EXPECT_TRUE(is_keyprint(key));
    EXPECT_FALSE(is_keyprint(key.substr(0, key.size() - 1)));
    EXPECT_FALSE(is_keyprint(key.substr(0, key.size() - 1) + 'B'));
}

} // namespace
} // namespace dc24h::adc
