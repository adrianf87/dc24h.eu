#include "dc24h/adc/parser.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace dc24h::adc;

void require(bool condition, const char* reason) {
    if (!condition) { throw std::runtime_error(reason); }
}
Message valid(std::string_view line) {
    auto parsed = parse_line(line);
    if (!std::holds_alternative<Message>(parsed)) { throw std::runtime_error("Rejected fixture: " + std::string(line)); }
    return std::get<Message>(std::move(parsed));
}
void invalid(std::string_view line, ParseError expected) {
    const auto parsed = parse_line(line);
    require(std::holds_alternative<ParseError>(parsed), "Malformed command accepted");
    require(std::get<ParseError>(parsed) == expected, "Wrong syntax diagnostic");
}

void base_layouts() {
    const std::vector<std::pair<std::string, std::size_t>> cases{
        {"ISTA 245 Denied\\soperation FCZ123 XX", 2},
        {"HSUP ADTIGR ADBASE RMTCP4 ADX9Z2", 4},
        {"ISID XYZ7 XYextension", 1},
        {"BINF ABC2 NIŁukasz DEOne\\stwo DE SS0 X9opaque", 0},
        {"DMSG ABC2 XYZ7 NIliteral\\stext PMABC2 ME1", 1},
        {"FSCH ABC2 +TCP4-TCP6 ANfirst ANsecond NO EXtxt", 0},
        {"URES A FNdir/file.txt SI12 TOsession XY", 0},
        {"DCTM ABC2 XYZ7 ADC/1.0 43123 token\\sone XY", 3},
        {"DRCM ABC2 XYZ7 ADC/1.0 another\\stoken XYvalue", 2},
        {"IGPA ABCDEFG234567 XYopaque", 1},
        {"HPAS ABCDEF234567 XY", 1},
        {"IQUI XYZ7 TL-1 MSBye\\sfor\\snow", 1},
        {"CGET file TTH/ABCDEFGHI 0 -1 RE1", 4},
        {"CGFI list /folder/ XYextension", 2},
        {"CSND binary2 resource\\sname 12 2048 XY", 4},
    };
    for (const auto& [line, count] : cases) {
        const auto parsed = valid(line);
        require(parsed.positional_count == count, "Incorrect positional layout");
        for (std::size_t i = count; i < parsed.parameters.size(); ++i) {
            require(parse_named_parameter(parsed.parameters[i]).has_value(), "Named field not accessible");
        }
    }
    for (const auto* line : {"BINF ABC2", "HSCH", "CRES"}) {
        require(valid(line).positional_count == 0, "Optional field-only command rejected");
    }
    for (const auto* line : {"ISTA", "ISTA 245", "HSUP", "ISID", "HMSG", "DCTM ABC2 XYZ7",
                             "DCTM ABC2 XYZ7 ADC/1.0 1234", "DRCM ABC2 XYZ7 ADC/1.0",
                             "IGPA", "HPAS", "IQUI", "CGET file /a 0", "CGFI file", "CSND file /a 0"}) {
        invalid(line, ParseError::missing_parameters);
    }
    // Unknown layouts cannot be inferred from strings that resemble field names.
    const auto extension = valid("DZ99 ABC2 XYZ7 lowercase NIarbitrary a 12 1Xfuture");
    require(!extension.positional_count && extension.parameters.size() == 5 &&
            extension.parameters[1] == "NIarbitrary", "Unknown command reinterpreted");
    require(!valid("HXYZ").positional_count, "Empty extension command rejected");
    // Syntax does not validate identities, state, hash lengths, field values or routing policy.
    require(valid("HINF IDopaque PDopaque SSnot-a-number").parameters.size() == 3, "Field semantics leaked into syntax");
    require(valid("HSID AAAA").positional_count == 1, "Parser enforced a command/type policy");
    require(valid("CSTA 999 Future\\scode").positional_count == 2, "Unknown status semantics rejected");
}

void named_fields_and_ownership() {
    auto parsed = valid(std::string("BINF ABC2 NIJoanna DEfirst\\sline\\nsecond\\\\path DE NI"));
    const auto first = parse_named_parameter(parsed.parameters[0]);
    const auto description = parse_named_parameter(parsed.parameters[1]);
    require(first && first->name == "NI" && first->value == "Joanna", "Named name/value split failed");
    require(description && description->value == "first line\nsecond\\path", "Named value decoded twice or not at all");
    require(parsed.parameters.size() == 4, "Repeated fields lost");
    require(parse_named_parameter(parsed.parameters[2])->value.empty() &&
            parse_named_parameter(parsed.parameters[3])->name == "NI", "Bare fields/cancellations lost");
    auto copied = parsed;
    parsed.parameters.clear();
    const auto moved = std::move(copied);
    require(parse_named_parameter(moved.parameters[0])->value == "Joanna" && moved.positional_count == 0,
            "Copy/move left borrowed parser storage");
    require(!parse_named_parameter("") && !parse_named_parameter("N") &&
            !parse_named_parameter("nIvalue") && !parse_named_parameter("1Avalue") &&
            !parse_named_parameter("N_value"), "Invalid named prefix accepted");
    require(parse_named_parameter("A9")->value.empty(), "Digit in second field-name position rejected");
    for (const auto* line : {"BINF ABC2 N", "BINF ABC2 niNick", "BINF ABC2 1Xvalue",
                             "HSCH ANone bare", "CRES FN/a 1Xbad", "HMSG hi bad",
                             "HMSG hi A\\svalue", "HINF N\\nvalue"}) {
        invalid(line, ParseError::invalid_named_parameter);
    }
    const auto literal = valid("HMSG NIlooks-like-a-field NI");
    require(literal.positional_count == 1 && literal.parameters[0] == "NIlooks-like-a-field",
            "Positional text misclassified as a named field");
}

void scalar_syntax() {
    require(parse_integer("9223372036854775807") == std::numeric_limits<std::int64_t>::max(), "INT64_MAX rejected");
    require(parse_integer("-9223372036854775808") == std::numeric_limits<std::int64_t>::min(), "INT64_MIN rejected");
    require(parse_integer("0") == 0 && parse_integer("-0") == 0 && parse_integer("0012") == 12,
            "Valid decimal integer rejected");
    const std::vector<std::string> bad{"", "-", "+1", " 1", "1 ", "1.0", "1e3", "0x10", "--1",
        "9223372036854775808", "-9223372036854775809", "99999999999999999999999999999",
        std::string("1\0", 2), "１２"};
    for (const auto& text : bad) { require(!parse_integer(text), "Invalid/overflow integer accepted"); }
    for (const auto* line : {"CGET file /a 1.5 -1", "CSND file /a 0 9223372036854775808",
                             "CGET file /a +1 10", "DCTM ABC2 XYZ7 ADC/1.0 port token"}) {
        invalid(line, ParseError::invalid_integer);
    }
    for (const auto* line : {"CGET fi-le /a 0 1", "CGFI file/type /a", "CSND fïle /a 0 1"}) {
        invalid(line, ParseError::invalid_transfer_type);
    }
    for (const auto* line : {"ISID BAD", "IQUI ABC0 MSbye"}) { invalid(line, ParseError::invalid_sid); }
    for (const auto* line : {"ISTA 24 no", "CSTA ABC no", "ISTA 2450 no"}) {
        invalid(line, ParseError::invalid_status_code);
    }
    // Numeric ranges/meanings belong to the consumer, including port and transfer policy.
    require(valid("DCTM ABC2 XYZ7 future-protocol -1 token").parameters[1] == "-1", "Port policy applied in syntax");
    require(valid("CGET file /a -12 -2").positional_count == 4, "Transfer policy applied in syntax");
}

void support_and_limits() {
    for (const auto* line : {"HSUP AD", "HSUP ADbase", "HSUP XYBASE", "HSUP AD1ASE", "HSUP ADBASEX",
                             "HSUP ADBASE RM", "HSUP ADBA\\sE", "HSUP ADBASE NIhub"}) {
        invalid(line, ParseError::invalid_support);
    }
    std::string support = "HSUP";
    std::string fields = "HINF";
    for (std::size_t i = 0; i < max_parameters; ++i) { support += " ADBASE"; fields += " XY"; }
    require(valid(support).positional_count == max_parameters && valid(fields).parameters.size() == max_parameters,
            "Exact argument budget rejected");
    invalid(support + " ADBASE", ParseError::too_many_parameters);
    invalid(fields + " XY", ParseError::too_many_parameters);
    for (const auto* prefix : {"HMSG hi", "BMSG ABC2 hi", "DMSG ABC2 XYZ7 hi", "EMSG ABC2 XYZ7 hi",
                               "FMSG ABC2 +TCP4 hi", "CMSG hi", "IMSG hi", "UMSG A hi"}) {
        std::string line(prefix);
        for (std::size_t i = 1; i < max_parameters; ++i) { line += " XY"; }
        require(valid(line).parameters.size() == max_parameters, "Routing consumed the argument budget");
        invalid(line + " XY", ParseError::too_many_parameters);
    }
    std::string selectors;
    for (std::size_t i = 0; i < max_feature_selectors; ++i) { selectors += (i % 2 == 0 ? "+TCP4" : "-TCP4"); }
    require(valid("FSCH ABC2 " + selectors + " ANx").features.size() == max_feature_selectors,
            "Exact selector budget rejected");
    invalid("FSCH ABC2 " + selectors + "+TCP4 ANx", ParseError::too_many_features);
    const auto repeated = valid("FSCH ABC2 +TCP4-TCP4 ANx ANx");
    require(repeated.features.size() == 2 && repeated.parameters.size() == 2, "Duplicates silently collapsed");
}

bool same(const ParseResult& left, const ParseResult& right) {
    if (left.index() != right.index()) { return false; }
    if (const auto* error = std::get_if<ParseError>(&left)) { return *error == std::get<ParseError>(right); }
    if (std::holds_alternative<KeepAlive>(left)) { return true; }
    const auto& a = std::get<Message>(left);
    const auto& b = std::get<Message>(right);
    if (a.type != b.type || a.command != b.command || a.kind != b.kind || a.source != b.source ||
        a.target != b.target || a.parameters != b.parameters || a.positional_count != b.positional_count ||
        a.features.size() != b.features.size()) { return false; }
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        if (a.features[i].required != b.features[i].required || a.features[i].name != b.features[i].name) { return false; }
    }
    return true;
}

void stream_preserves_syntax() {
    const std::string wire = "HSUP ADBASE RMTCP4\nBINF ABC2 NIĄcki DE\\s DE\nHMSG NItext PMXYZ7\n"
        "BINF ABC2 x\nHSUP ADwrong\nISID ABC0\nCGET file /a 1e3 -1\n"
        "FSCH ABC2 +TCP4-TCP6 ANą ANtwo\nDZ99 ABC2 XYZ7 lower x\n\nCSND file /b 0 12\n";
    std::vector<ParseResult> expected;
    StreamDecoder baseline;
    baseline.feed(wire, [&](ParseResult result) { expected.push_back(std::move(result)); });
    require(expected.size() == 11, "Unexpected result count");
    require(std::get<ParseError>(expected[3]) == ParseError::invalid_named_parameter &&
            std::get<ParseError>(expected[4]) == ParseError::invalid_support, "New syntax errors not delivered");
    for (std::size_t split = 0; split <= wire.size(); ++split) {
        StreamDecoder decoder;
        std::vector<ParseResult> actual;
        const StreamDecoder::Sink sink = [&](ParseResult result) { actual.push_back(std::move(result)); };
        decoder.feed(std::string_view(wire).substr(0, split), sink);
        decoder.feed(std::string_view(wire).substr(split), sink);
        require(!decoder.finish() && actual.size() == expected.size(), "Split changed framing/recovery");
        for (std::size_t i = 0; i < expected.size(); ++i) {
            require(same(expected[i], actual[i]), "Split changed owned header/argument data");
        }
    }
    StreamDecoder decoder;
    std::vector<ParseResult> bytes;
    for (const char& byte : wire) {
        decoder.feed(std::string_view(&byte, 1), [&](ParseResult result) { bytes.push_back(std::move(result)); });
    }
    require(bytes.size() == expected.size(), "Bytewise stream changed framing");
    for (std::size_t i = 0; i < expected.size(); ++i) { require(same(expected[i], bytes[i]), "Bytewise stream changed values"); }
}
} // namespace

int main() {
    try {
        base_layouts();
        named_fields_and_ownership();
        scalar_syntax();
        support_and_limits();
        stream_preserves_syntax();
        std::cout << "PASS: BASE layouts, named fields, integers, limits, ownership and full-result stream recovery\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
