#include "dc24h/adc/parser.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace dc24h::adc;

void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}
Message message(std::string_view line) {
    auto result = parse_line(line);
    require(std::holds_alternative<Message>(result), "Valid frame rejected");
    return std::get<Message>(std::move(result));
}
void error(std::string_view line, ParseError expected) {
    const auto result = parse_line(line);
    require(std::holds_alternative<ParseError>(result), "Invalid frame accepted");
    require(std::get<ParseError>(result) == expected, "Incorrect parse error category");
}

void headers_and_commands() {
    const auto sup = message("HSUP ADBASE ADTIGR ADUCM0");
    require(sup.type == MessageType::hub && sup.kind == Command::sup && sup.command == "SUP",
            "HSUP not recognized");
    require(!sup.source && !sup.target && sup.parameters.size() == 3, "Hub header parsed incorrectly");
    const auto broadcast = message("BINF ABC2 NIAlicja SS42 NI");
    require(broadcast.source == "ABC2" && broadcast.parameters[2] == "NI", "Broadcast fields lost");
    const auto direct = message("DMSG ABC2 XYZ7 tekst PMABC2");
    require(direct.source == "ABC2" && direct.target == "XYZ7" && direct.parameters[0] == "tekst",
            "Direct route parsed incorrectly");
    require(message("EMSG ABC2 XYZ7 hi").type == MessageType::echo, "Echo header rejected");
    require(message("CINF IDABCDEFG").type == MessageType::client, "Client header rejected");
    require(message("ISID ABC2").type == MessageType::info, "Info header rejected");
    const auto udp = message("URES ABCDEFG234567 FNtest SI0");
    require(udp.source == "ABCDEFG234567" && udp.parameters.size() == 2, "UDP CID not preserved");
    require(message("URES A FNtest").source == "A", "CID incorrectly tied to TIGR length");
    const auto filtered = message("FSCH ABC2 +TCP4-TCP6+UCM0 ANplik");
    require(filtered.features.size() == 3 && filtered.features[0].required &&
            !filtered.features[1].required && filtered.features[2].name == "UCM0", "Feature selectors invalid");
    const std::array names{"STA", "SUP", "SID", "INF", "MSG", "SCH", "RES", "CTM",
                           "RCM", "GPA", "PAS", "QUI", "GET", "GFI", "SND"};
    for (const auto* name : names) {
        require(recognize_command(name) != Command::unknown, "Missing base command classification");
    }
    const auto extension = message("HZ99 arbitrary XYunknown");
    require(extension.kind == Command::unknown && extension.command == "Z99" &&
            extension.parameters.size() == 2, "Unknown extension not preserved");
    require(recognize_command("sup") == Command::unknown, "Command comparison not case sensitive");
    // Command-specific semantics and state checks deliberately belong to a later layer.
    require(message("HSUP").parameters.empty(), "Parser performed semantic validation");
    require(std::holds_alternative<KeepAlive>(parse_line("")), "Empty LF not a keepalive");
}

void escaping_and_unicode() {
    const auto result = message("BMSG ABC2 Zażółć\\sgęślą\\nŚcieżka:\\\\plik PMXYZ7");
    require(result.parameters[0] == "Zażółć gęślą\nŚcieżka:\\plik", "Escape/Unicode decoding failed");
    require(message("HMSG 😀").parameters[0] == "😀", "Four-byte UTF-8 rejected");
    require(message("HMSG x\xcc\x81").parameters[0] == "x\xcc\x81", "Valid NFC combining mark rejected");
    error("HMSG e\xcc\x81", ParseError::non_normalized_text);
    const std::vector<std::string> invalid_utf8{
        "\x80", "\xc0\xaf", "\xe0\x80\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80",
        "\xf5\x80\x80\x80", "\xff", "\xe2\x82", "\xc2x"
    };
    for (const auto& text : invalid_utf8) { error("HMSG " + text, ParseError::invalid_utf8); }
    for (char control : {'\0', '\t', '\r', '\n', '\x1b', '\x7f'}) {
        error("HMSG text" + std::string(1, control), ParseError::control_character);
    }
    error("HMSG bad\\tvalue", ParseError::invalid_escape);
    error("HMSG bad\\", ParseError::invalid_escape);
    error("HMSG bad\\r", ParseError::invalid_escape);
}

void malformed_and_limits() {
    for (const auto* line : {"H", "HSU", "hsup", "Hsup", "H1UP", "HSuP", "XMSG", "HSUPX"}) {
        error(line, ParseError::invalid_header);
    }
    for (const auto* line : {"HSUP ", "HSUP  ADBASE", "HSUP ADBASE ", "HSUP ADBASE  ADTIGR"}) {
        error(line, ParseError::invalid_separator);
    }
    for (const auto* line : {"BMSG", "BMSG ABC", "BMSG ABCDE", "BMSG abc2 x", "BMSG ABC0 x",
                             "DMSG ABC2", "EMSG ABC2 XYZ1 x", "FSCH AAAA\\s +TCP4"}) {
        error(line, ParseError::invalid_sid);
    }
    for (const auto* line : {"URES", "URES ABC1", "URES abcd", "URES ABC=", "URES ABC\\s"}) {
        error(line, ParseError::invalid_cid);
    }
    for (const auto* line : {"FSCH ABC2", "FSCH ABC2 TCP4", "FSCH ABC2 +tcp4", "FSCH ABC2 +TCP",
                             "FSCH ABC2 +TCP44", "FSCH ABC2 +TCP4-", "FSCH ABC2 +1CP4"}) {
        error(line, ParseError::invalid_features);
    }
    const std::string maximum = "HMSG " + std::string(max_frame_bytes - 5, 'a');
    require(message(maximum).parameters[0].size() == max_frame_bytes - 5, "Maximum length rejected");
    error(maximum + 'a', ParseError::frame_too_long);
    for (const auto* prefix : {"HXYZ", "BXYZ ABC2", "DXYZ ABC2 XYZ7", "FXYZ ABC2 +TCP4"}) {
        std::string line(prefix);
        for (std::size_t i = 0; i < max_parameters; ++i) { line += " a"; }
        require(message(line).parameters.size() == max_parameters, "Parameter limit rejected too early");
        error(line + " a", ParseError::too_many_parameters);
    }
}

std::vector<int> decode(std::string_view input, std::size_t split) {
    StreamDecoder decoder;
    std::vector<int> events;
    const StreamDecoder::Sink collect = [&](ParseResult result) {
        if (const auto* parsed = std::get_if<Message>(&result)) {
            events.push_back(static_cast<int>(parsed->kind));
        } else if (std::holds_alternative<KeepAlive>(result)) {
            events.push_back(-1);
        } else {
            events.push_back(-2 - static_cast<int>(std::get<ParseError>(result)));
        }
    };
    decoder.feed(input.substr(0, split), collect);
    decoder.feed(input.substr(split), collect);
    require(!decoder.finish(), "Unexpected partial frame at EOF");
    return events;
}

void stream_boundaries() {
    const std::string input = "HSUP ADBASE ADTIGR\nBMSG ABC2 Zażółć\\switaj\\n😀\n\n"
                              "HMSG broken\\q\nFSCH ABC2 +TCP4-TCP6 ANx\nHZ99 value\n";
    const auto expected = decode(input, 0);
    require(expected.size() == 6 && expected[0] == static_cast<int>(Command::sup) &&
            expected[1] == static_cast<int>(Command::msg) && expected[2] == -1 &&
            expected[3] == -2 - static_cast<int>(ParseError::invalid_escape) &&
            expected[4] == static_cast<int>(Command::sch) && expected[5] == static_cast<int>(Command::unknown),
            "Incorrect ordered stream results");
    for (std::size_t split = 0; split <= input.size(); ++split) {
        require(decode(input, split) == expected, "TCP split changed parsing results");
    }
    StreamDecoder decoder;
    std::size_t messages = 0;
    std::size_t errors = 0;
    std::size_t keepalives = 0;
    const StreamDecoder::Sink collect = [&](ParseResult result) {
        messages += std::holds_alternative<Message>(result) ? 1U : 0U;
        errors += std::holds_alternative<ParseError>(result) ? 1U : 0U;
        keepalives += std::holds_alternative<KeepAlive>(result) ? 1U : 0U;
    };
    // Complete frames at the exact limit, then one oversize frame across many reads.
    const auto maximum = "HMSG " + std::string(max_frame_bytes - 5, 'a');
    for (char c : maximum) { decoder.feed(std::string_view(&c, 1), collect); }
    require(decoder.buffered_bytes() == max_frame_bytes && messages == 0, "Frame completed without LF");
    decoder.feed("\n", collect);
    require(messages == 1, "Exact limit frame not delivered");
    for (int i = 0; i < 1024; ++i) {
        decoder.feed(std::string(1024, 'x'), collect);
        require(decoder.buffered_bytes() <= max_frame_bytes, "Unbounded pending frame");
    }
    require(errors == 1 && decoder.buffered_bytes() == 0, "Oversize frame not discarded once");
    decoder.feed("\nHSUP ADBASE\n\nHMSG partial", collect);
    require(messages == 2 && keepalives == 1 && errors == 1, "Resynchronization after oversize failed");
    require(decoder.finish() == ParseError::truncated_frame && !decoder.finish(), "EOF not idempotent");
    decoder.feed("HSUP ADBASE\n", collect);
    require(messages == 3, "Decoder reset after EOF failed");
    decoder.feed(std::string(max_frame_bytes + 1, 'x'), collect);
    require(!decoder.finish() && errors == 2, "Oversize EOF counted twice");
}

void randomized_chunks() {
    // Deterministic malformed-byte corpus; chunking must not affect recovery/results.
    std::mt19937 generator(24013);
    std::string wire;
    for (int i = 0; i < 4000; ++i) {
        const auto size = generator() % 80;
        for (unsigned int j = 0; j < size; ++j) { wire.push_back(static_cast<char>(generator() % 256)); }
        wire += "\nHSUP ADBASE\n";
    }
    const auto whole = decode(wire, 0);
    StreamDecoder decoder;
    std::vector<int> chunked;
    const StreamDecoder::Sink collect = [&](ParseResult result) {
        if (const auto* parsed = std::get_if<Message>(&result)) {
            chunked.push_back(static_cast<int>(parsed->kind));
        } else if (std::holds_alternative<KeepAlive>(result)) {
            chunked.push_back(-1);
        } else {
            chunked.push_back(-2 - static_cast<int>(std::get<ParseError>(result)));
        }
    };
    for (std::size_t offset = 0; offset < wire.size();) {
        const auto length = std::min<std::size_t>(1 + generator() % 97, wire.size() - offset);
        decoder.feed(std::string_view(wire).substr(offset, length), collect);
        offset += length;
    }
    require(!decoder.finish() && whole == chunked, "Random chunking changed validation results");
}
} // namespace

int main() {
    try {
        headers_and_commands();
        escaping_and_unicode();
        malformed_and_limits();
        stream_boundaries();
        randomized_chunks();
        std::cout << "PASS: ADC headers, commands, escapes, UTF-8/NFC, limits, streams and recovery\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "FAIL: " << failure.what() << '\n';
        return 1;
    }
}
