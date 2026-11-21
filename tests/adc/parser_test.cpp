#include "test_messages.hpp"

#include <gtest/gtest.h>

#include <random>
#include <utility>

namespace dc24h::adc {
namespace {

TEST(AdcParser, ParsesAllMessageHeadersAndKeepsUnknownCommandNames) {
    EXPECT_TRUE(std::holds_alternative<KeepAlive>(parse_frame("\n")));
    EXPECT_EQ(test::decode("CSUP ADBASE\n").type, MessageType::client);
    EXPECT_EQ(test::decode("ISID ABCD\n").type, MessageType::information);

    const auto broadcast = test::decode("BINF A27Z NIguest\n");
    EXPECT_EQ(broadcast.type, MessageType::broadcast);
    EXPECT_EQ(broadcast.sender_sid, "A27Z");
    EXPECT_EQ(broadcast.parameters, std::vector<std::string>{"NIguest"});

    for (const auto &[wire, type] : {std::pair{"DMSG ABCD ZX76 hello\n", MessageType::direct},
                                     std::pair{"EMSG ABCD ZX76 hello\n", MessageType::echo}}) {
        const auto message = test::decode(wire);
        EXPECT_EQ(message.type, type);
        EXPECT_EQ(message.sender_sid, "ABCD");
        EXPECT_EQ(message.target_sid, "ZX76");
    }

    const auto feature = test::decode("FMSG ABCD +TCP4-UDP6 hello\n");
    EXPECT_EQ(feature.type, MessageType::feature);
    ASSERT_EQ(feature.selectors.size(), 2U);
    EXPECT_EQ(feature.selectors[0].name, "TCP4");
    EXPECT_TRUE(feature.selectors[0].required);
    EXPECT_EQ(feature.selectors[1].name, "UDP6");
    EXPECT_FALSE(feature.selectors[1].required);

    const auto udp = test::decode("URES " + std::string(test::cid) + " FNtest\n");
    EXPECT_EQ(udp.type, MessageType::udp);
    EXPECT_EQ(udp.sender_cid, test::cid);
    EXPECT_EQ(test::decode("URES MZXW6 FNtest\n").sender_cid, "MZXW6");

    const auto unknown = test::decode("HQ1Z parameter\n");
    EXPECT_EQ(unknown.type, MessageType::hub);
    EXPECT_EQ(unknown.command, "Q1Z");
    EXPECT_EQ(unknown.fourcc(), "HQ1Z");
}

TEST(AdcParser, RejectsMalformedFramesAndRoutingFields) {
    const std::pair<std::string_view, ParseError> cases[] = {
        {"", ParseError::incomplete_frame},
        {"HSUP ADBASE", ParseError::incomplete_frame},
        {"HSUP\nHSUP\n", ParseError::multiple_frames},
        {"HIN\n", ParseError::invalid_header},
        {"XINF\n", ParseError::invalid_header},
        {"HInF\n", ParseError::invalid_header},
        {"H1NF\n", ParseError::invalid_header},
        {" HINF\n", ParseError::invalid_header},
        {"HINF\r\n", ParseError::invalid_header},
        {"BINF\n", ParseError::invalid_sid},
        {"BINF A01D\n", ParseError::invalid_sid},
        {"DMSG ABCD\n", ParseError::invalid_sid},
        {"EMSG ABCD xyz2\n", ParseError::invalid_sid},
        {"URES AB FNtest\n", ParseError::invalid_cid},
        {"URES abc FNtest\n", ParseError::invalid_cid},
        {"FMSG ABCD\n", ParseError::invalid_feature},
        {"FMSG ABCD TCP4\n", ParseError::invalid_feature},
        {"FMSG ABCD +tcp4\n", ParseError::invalid_feature},
        {"FMSG ABCD +TCP4-\n", ParseError::invalid_feature},
        {"HINF \n", ParseError::invalid_parameter},
        {"HSUP  ADBASE\n", ParseError::invalid_parameter},
        {"HMSG a\\x\n", ParseError::invalid_escape},
        {"HMSG a\\\n", ParseError::invalid_escape},
    };
    for (const auto &[input, error] : cases) {
        SCOPED_TRACE(input);
        const auto result = parse_frame(input);
        ASSERT_TRUE(std::holds_alternative<ParseError>(result));
        EXPECT_EQ(std::get<ParseError>(result), error);
    }
}

TEST(AdcParser, EscapesSpaceNewlineAndBackslash) {
    constexpr std::string_view plain = "two words\nback\\slash caf\xc3\xa9";
    const auto wire = escape_parameter(plain);
    EXPECT_EQ(wire, "two\\swords\\nback\\\\slash\\scaf\xc3\xa9");
    EXPECT_EQ(std::get<std::string>(unescape_parameter(wire)), plain);
    EXPECT_EQ(std::get<ParseError>(unescape_parameter("two words")), ParseError::invalid_parameter);
    EXPECT_EQ(std::get<ParseError>(unescape_parameter("two\nwords")),
              ParseError::invalid_parameter);
    EXPECT_EQ(std::get<ParseError>(unescape_parameter("")), ParseError::invalid_parameter);
}

TEST(AdcParser, ValidatesUtf8AndNfcAfterUnescaping) {
    EXPECT_EQ(test::decode("HMSG caf\xc3\xa9\\s\xf0\x9f\x98\x83\n").parameters.front(),
              "caf\xc3\xa9 \xf0\x9f\x98\x83");
    EXPECT_EQ(test::decode("HMSG \\s\xcc\x81\n").parameters.front(), " \xcc\x81");
    EXPECT_EQ(std::get<ParseError>(parse_frame("HMSG cafe\xcc\x81\n")), ParseError::non_nfc);
    for (const std::string_view invalid : {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80",
                                           "\xe2\x82", "\x80", "\xf8\x88\x80\x80\x80"}) {
        EXPECT_EQ(std::get<ParseError>(unescape_parameter(invalid)), ParseError::invalid_utf8);
    }
    const std::string binary = std::string("HMSG two") + '\0' + "words\n";
    EXPECT_EQ(test::decode(binary).parameters.front(), std::string("two\0words", 9));
}

TEST(AdcParser, EnforcesFrameLengthAndParameterLimits) {
    const Limits limits{.frame_bytes = 8, .parameters = 2, .parameter_bytes = 2};
    EXPECT_TRUE(std::holds_alternative<Message>(parse_frame("HMSG a b\n", limits)));
    EXPECT_EQ(std::get<ParseError>(parse_frame("HMSG aa b\n", limits)), ParseError::frame_too_long);
    EXPECT_EQ(std::get<ParseError>(parse_frame("HMSG abc\n", limits)),
              ParseError::parameter_too_long);
    EXPECT_EQ(std::get<ParseError>(parse_frame("HMSG a b c\n", {.parameters = 2})),
              ParseError::too_many_parameters);
    EXPECT_EQ(std::get<ParseError>(parse_frame("FMSG ABCD +TCP4-UDP6\n", {.parameters = 1})),
              ParseError::too_many_parameters);
    EXPECT_EQ(std::get<ParseError>(parse_frame("HMSG a\\sb\n", {.parameter_bytes = 3})),
              ParseError::parameter_too_long);
}

TEST(AdcFrameDecoder, HandlesFragmentedUtf8EscapesAndCoalescedFrames) {
    FrameDecoder decoder;
    const auto first = decoder.consume("BMSG ABCD caf\xc3");
    EXPECT_EQ(first.consumed, 14U);
    EXPECT_FALSE(first.frame);
    const auto second = decoder.consume("\xa9\\");
    EXPECT_EQ(second.consumed, 2U);
    EXPECT_FALSE(second.frame);
    EXPECT_EQ(decoder.buffered_bytes(), 16U);

    std::string_view remaining = "sfin\nISID ABCD\n\n";
    const auto text = decoder.consume(remaining);
    EXPECT_EQ(text.consumed, 5U);
    ASSERT_TRUE(text.frame);
    EXPECT_EQ(std::get<Message>(*text.frame).parameters.front(), "caf\xc3\xa9 fin");
    remaining.remove_prefix(text.consumed);

    const auto sid = decoder.consume(remaining);
    EXPECT_EQ(sid.consumed, 10U);
    ASSERT_TRUE(sid.frame);
    EXPECT_EQ(parse_sid(std::get<Message>(*sid.frame)), "ABCD");
    remaining.remove_prefix(sid.consumed);
    EXPECT_TRUE(std::holds_alternative<KeepAlive>(*decoder.consume(remaining).frame));
    EXPECT_FALSE(decoder.finish());
}

TEST(AdcFrameDecoder, RejectsOversizedLineOnceAndResynchronizes) {
    FrameDecoder decoder({.frame_bytes = 8});
    EXPECT_FALSE(decoder.consume("HMSG 123").frame);
    EXPECT_EQ(decoder.buffered_bytes(), 8U);
    const auto oversized = decoder.consume(std::string(8192, 'x'));
    EXPECT_EQ(oversized.consumed, 8192U);
    ASSERT_TRUE(oversized.frame);
    EXPECT_EQ(std::get<ParseError>(*oversized.frame), ParseError::frame_too_long);
    EXPECT_EQ(decoder.buffered_bytes(), 0U);
    EXPECT_FALSE(decoder.consume("more").frame);

    std::string_view remaining = "tail\nHNEW\n";
    const auto discarded = decoder.consume(remaining);
    EXPECT_FALSE(discarded.frame);
    EXPECT_EQ(discarded.consumed, 5U);
    remaining.remove_prefix(discarded.consumed);
    EXPECT_EQ(std::get<Message>(*decoder.consume(remaining).frame).command, "NEW");

    const auto too_long = decoder.consume("HMSG abcdefgh\n");
    EXPECT_EQ(std::get<ParseError>(*too_long.frame), ParseError::frame_too_long);
    EXPECT_TRUE(std::holds_alternative<KeepAlive>(*decoder.consume("\n").frame));
}

TEST(AdcFrameDecoder, ReportsTruncatedInputAtEndOfStreamAndResets) {
    FrameDecoder decoder;
    EXPECT_EQ(decoder.consume("").consumed, 0U);
    EXPECT_FALSE(decoder.consume("HSUP ADBASE").frame);
    EXPECT_EQ(decoder.finish(), ParseError::incomplete_frame);
    EXPECT_FALSE(decoder.finish());
    EXPECT_EQ(std::get<Message>(*decoder.consume("HSUP ADBASE\n").frame).command, "SUP");
}

TEST(AdcFrameDecoder, MakesBoundedProgressOnDeterministicRandomBytes) {
    const Limits limits{.frame_bytes = 64, .parameters = 8, .parameter_bytes = 16};
    std::mt19937 random(7345);
    for (std::size_t trial = 0; trial < 512; ++trial) {
        std::string data(trial, '\0');
        for (auto &byte : data) {
            byte = static_cast<char>(random() & 0xffU);
        }
        FrameDecoder decoder(limits);
        std::string_view remaining = data;
        while (!remaining.empty()) {
            const auto chunk = remaining.substr(0, 1U + random() % 19U);
            const auto result = decoder.consume(chunk);
            ASSERT_GT(result.consumed, 0U);
            ASSERT_LE(result.consumed, chunk.size());
            EXPECT_LE(decoder.buffered_bytes(), limits.frame_bytes);
            remaining.remove_prefix(result.consumed);
        }
        static_cast<void>(decoder.finish());
        EXPECT_EQ(decoder.buffered_bytes(), 0U);
    }
}

} // namespace
} // namespace dc24h::adc
