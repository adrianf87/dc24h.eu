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

} // namespace
} // namespace dc24h::adc
