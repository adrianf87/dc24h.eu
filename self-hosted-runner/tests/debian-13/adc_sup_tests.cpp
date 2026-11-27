#include "dc24h/adc/sup.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using namespace dc24h::adc;
constexpr auto in = Direction::from_client;
constexpr auto out = Direction::to_client;
void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}
Message parse(std::string_view text) {
    auto result = parse_line(text);
    require(std::holds_alternative<Message>(result), "Invalid test frame");
    return std::get<Message>(std::move(result));
}
void accept(ProtocolConnection& connection, Direction direction, std::string_view text) {
    require(!connection.observe(direction, parse(text)), "Expected accepted protocol event");
}
void support_error(ProtocolConnection& connection, Direction direction, std::string_view text,
                   SupportError expected) {
    const auto phase = connection.state();
    const auto features = connection.support().peer_features();
    const auto hash = connection.support().selected_hash();
    const std::string sid(connection.sid());
    require(connection.observe(direction, parse(text)) == ProtocolError{expected}, "Wrong SUP error");
    require(connection.state() == phase && connection.sid() == sid &&
            connection.support().peer_features() == features && connection.support().selected_hash() == hash,
            "Rejected SUP mutated connection");
}
void normal(ProtocolConnection& connection) {
    accept(connection, in, "HSUP ADBASE ADTIGR ADTCP4");
    require(connection.support().initial_reply().has_value(), "Reply must exist");
    require(!connection.observe(out, *connection.support().initial_reply()), "Reply must pass state gate");
    accept(connection, out, "ISID ABCD");
    accept(connection, in, "BINF ABCD NIalice");
    require(!connection.approve_identity(IdentityMode::guest), "Fixture identity decision");
}
std::string feature(unsigned value) {
    std::string result = "Q000";
    for (unsigned i = 0; i < 3; ++i) {
        result[3 - i] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    return result;
}

void negotiation_and_reply() {
    SupportNegotiation support;
    require(!support.selected_hash() && !support.initial_reply() && !support.initial_reply_line(), "No early reply");
    require(support.update_peer(parse("HSUP ADTCP4")) == SupportError::invalid_sequence, "No pre-login updates");
    require(!support.accept_initial(parse("HSUP ADTCP4 ADTIGR ADBASE ADX123 ADTIGR")), "Initial SUP");
    require(support.selected_hash() == SessionHash::tiger192, "Only common registered hash must be chosen");
    require(hash_feature(*support.selected_hash()) == "TIGR" && hash_bytes(*support.selected_hash()) == 24,
            "Tiger metadata");
    require(support.peer_features() == std::vector<std::string>({"BASE", "TCP4", "TIGR", "X123"}),
            "Canonical owned peer set");
    require(!support.peer_supports("TCP6") && support.peer_supports("X123"), "Peer feature lookup");
    require(support.initial_reply_line() == "ISUP ADBASE ADTIGR\n", "Exact default response");
    const auto reply = *support.initial_reply();
    require(reply.type == MessageType::info && reply.kind == Command::sup && !reply.source && !reply.target &&
            reply.features.empty() && reply.positional_count == 2, "Owned reply layout");
    const auto wire = *support.initial_reply_line();
    require(parse(std::string_view(wire).substr(0, wire.size()-1)).parameters == reply.parameters, "Reply round trip");
    require(support.accept_initial(parse("HSUP ADBASE ADTIGR")) == SupportError::invalid_sequence, "No renegotiation");

    // Local optional features are advertised independently of peer features;
    // peer-only extensions are never echoed into local capabilities.
    SupportNegotiation configured({{"ZLIF", "TIGR", "BASE", "BZIP", "BASE"}});
    require(!configured.accept_initial(parse("HSUP ADBASE ADTIGR ADUCMD")), "Configured profile");
    require(configured.initial_reply_line() == "ISUP ADBASE ADTIGR ADBZIP ADZLIF\n", "Deterministic local order");
    require(!configured.local_features().empty() && !configured.peer_supports("BZIP"), "Separate advertisements");
    auto owned = *configured.initial_reply();
    owned.parameters.clear();
    require(configured.initial_reply()->parameters.size() == 4, "Reply must not alias negotiation");
}

void failures_and_validation() {
    for (const auto& features : std::vector<std::vector<std::string>>{
            {}, {"BASE"}, {"TIGR"}, {"BASE", "TIGR", "bad!"},
            {"BASE", "TIGR", "TOOLONG"}, {"BASE", "TIGR", "1ABC"},
            std::vector<std::string>(129, "BASE")}) {
        bool failed = false;
        try { const SupportNegotiation invalid(SupportPolicy{features}); }
        catch (const std::invalid_argument&) { failed = true; }
        require(failed, "Invalid local policy accepted");
    }
    ProtocolConnection connection;
    for (const auto* line : {"HSUP ADTIGR", "HSUP RMTIGR", "HSUP ADBASE RMBASE ADTIGR"}) {
        support_error(connection, in, line, SupportError::missing_base);
    }
    for (const auto* line : {"HSUP ADBASE", "HSUP ADBASE ADXXXX", "HSUP ADBASE ADTIGR RMTIGR"}) {
        support_error(connection, in, line, SupportError::no_common_hash);
    }
    for (const auto* line : {"ISUP ADBASE ADTIGR", "CSUP ADBASE ADTIGR", "BSUP ABCD ADBASE ADTIGR"}) {
        support_error(connection, in, line, SupportError::invalid_message);
    }
    require(connection.observe(out, parse("ISID ABCD")) == ProtocolError{StateError::invalid_sequence},
            "Rejected negotiation must not approve state");
    require(support_status_code(SupportError::missing_base) == "245" &&
            support_status_code(SupportError::no_common_hash) == "247" &&
            support_status_code(SupportError::feature_limit) == "240", "Fixed status codes");

    const auto valid = parse("HSUP ADBASE ADTIGR");
    for (unsigned mode = 0; mode < 8; ++mode) {
        auto bad = valid;
        switch (mode) {
        case 0: bad.command = "INF"; break;
        case 1: bad.kind = Command::inf; break;
        case 2: bad.source = "ABCD"; break;
        case 3: bad.target = "ABCD"; break;
        case 4: bad.features.push_back({true, "TCP4"}); break;
        case 5: bad.parameters = {"ADBASE", "ADTIGR", "XXTCP4"}; break;
        case 6: bad.parameters.clear(); break;
        default: bad.parameters.push_back("ADbad!"); break;
        }
        SupportNegotiation standalone;
        require(standalone.accept_initial(bad) == SupportError::invalid_message, "Hand-built SUP guard");
        require(!standalone.selected_hash() && standalone.peer_features().empty(), "Bad input retained data");
    }
    auto too_many = valid;
    too_many.parameters.resize(129, "ADBASE");
    SupportNegotiation standalone;
    require(standalone.accept_initial(too_many) == SupportError::invalid_message, "Token budget guard");
    accept(connection, in, "HSUP ADBASE ADTIGR"); // Error recovery with no prior mutation.
}

void updates_and_budgets() {
    SupportPolicy largest_policy;
    std::string largest_initial = "HSUP ADBASE ADTIGR";
    for (unsigned i = 0; i < 126; ++i) {
        largest_policy.features.push_back(feature(i));
        largest_initial += " AD" + feature(i);
    }
    SupportNegotiation largest(std::move(largest_policy));
    require(!largest.accept_initial(parse(largest_initial)), "Exact initial/local feature bound");
    const auto line = *largest.initial_reply_line();
    require(line.size() <= max_frame_bytes && line.back() == '\n' &&
            parse(std::string_view(line).substr(0, line.size()-1)).parameters.size() == max_parameters,
            "Largest response must fit one parsed SUP frame");
    ProtocolConnection connection;
    normal(connection);
    accept(connection, in, "HSUP ADTCP6 RMTCP4 ADTCP6 RMX123 ADX123 RMX123");
    require(connection.support().peer_features() == std::vector<std::string>({"BASE", "TCP6", "TIGR"}),
            "Ordered idempotent AD/RM updates");
    accept(connection, in, "HSUP RMBASE ADBASE RMTIGR ADTIGR");
    require(connection.support().selected_hash() == SessionHash::tiger192, "Net no-op keeps session hash");
    support_error(connection, in, "HSUP ADUCMD RMBASE", SupportError::missing_base);
    support_error(connection, in, "HSUP ADUCMD RMTIGR", SupportError::session_hash_removed);
    require(!connection.support().peer_supports("UCMD"), "Failed update leaked partial changes");
    support_error(connection, out, "ISUP ADUCMD", SupportError::local_update_unsupported);

    ProtocolConnection bounded;
    normal(bounded);
    accept(bounded, in, "HSUP RMTCP4");
    std::string updates = "HSUP";
    for (unsigned i = 0; i < 126; ++i) { updates += " AD" + feature(i); }
    accept(bounded, in, updates);
    require(bounded.support().peer_features().size() == max_support_features, "Exact cumulative limit");
    accept(bounded, in, "HSUP ADQ000"); // Duplicate does not consume capacity.
    support_error(bounded, in, "HSUP ADQ999", SupportError::feature_limit);
    support_error(bounded, in, "HSUP ADQ999 RMQ000", SupportError::feature_limit);
    accept(bounded, in, "HSUP RMQ000 ADQ999"); // Remove first frees capacity.
    require(!bounded.support().peer_supports("Q000") && bounded.support().peer_supports("Q999"), "Slot reuse");
    const auto snapshot = bounded.support().peer_features();
    auto malformed = parse("HSUP RMQ001");
    malformed.parameters.push_back("BAD");
    require(bounded.observe(in, malformed) == ProtocolError{SupportError::invalid_message}, "Invalid final token");
    require(bounded.support().peer_features() == snapshot, "Invalid update must roll back removal");
}

void state_composition() {
    ProtocolConnection connection;
    support_error(connection, out, "ISUP ADBASE ADTIGR", SupportError::reply_mismatch);
    accept(connection, in, "HSUP ADBASE ADTIGR ADX123");
    require(connection.state() == ConnectionState::protocol, "Negotiation is not completed output");
    require(connection.observe(in, parse("BINF ABCD NIearly")) == ProtocolError{StateError::invalid_state},
            "Early INF bypass");
    support_error(connection, in, "HSUP ADBASE ADTIGR", SupportError::invalid_sequence);
    for (const char* text : {"ISUP ADTIGR ADBASE", "ISUP ADBASE", "ISUP ADBASE ADTIGR ADX123"}) {
        support_error(connection, out, text, SupportError::reply_mismatch);
    }
    accept(connection, out, "ISUP ADBASE ADTIGR");
    require(connection.state() == ConnectionState::protocol, "Still waiting for SID send");
    require(connection.observe(out, parse("ISUP ADBASE ADTIGR")) == ProtocolError{StateError::invalid_sequence},
            "Repeated ISUP");
    accept(connection, out, "ISID ABCD");
    require(connection.state() == ConnectionState::identify, "Ordered SID transition");
    require(connection.observe(in, parse("HSUP ADTCP6")) == ProtocolError{StateError::invalid_state},
            "No SUP update in IDENTIFY");
    accept(connection, in, "BINF ABCD NIregistered");
    require(!connection.approve_identity(IdentityMode::password_required), "Password path approval");
    require(connection.observe(in, parse("HSUP ADTCP6")) == ProtocolError{StateError::invalid_state},
            "No SUP update in VERIFY");
    accept(connection, out, "IGPA challenge");
    accept(connection, in, "HPAS response");
    require(!connection.approve_password(), "Password outcome");
    accept(connection, in, "HSUP ADTCP6");
    require(connection.observe(in, parse("BMSG EFGH spoof")) == ProtocolError{StateError::wrong_source},
            "Composition must preserve source gate");
    accept(connection, out, "IQUI ABCD");
    require(connection.observe(in, parse("HSUP ADTCP4")) == ProtocolError{StateError::disconnected}, "Closed SUP");
    require(connection.keepalive() == StateError::disconnected, "Closed keepalive");
    require(connection.approve_identity(IdentityMode::guest) == StateError::disconnected, "Closed approval");
    ProtocolConnection other;
    require(!other.support().selected_hash() && other.support().peer_features().empty(), "Instance isolation");
    normal(other);
    require(!other.support().peer_supports("X123"), "Features must not leak across sessions");
}

void every_stream_split() {
    const std::string wire = "HSUP ADX123 RMX123 ADBASE ADTIGR\nHSUP ADTCP4\n";
    for (std::size_t split = 0; split <= wire.size(); ++split) {
        ProtocolConnection connection;
        StreamDecoder decoder;
        unsigned count = 0;
        const auto sink = [&](ParseResult result) {
            require(std::holds_alternative<Message>(result), "SUP stream must parse");
            if (count++ == 0) { require(!connection.observe(in, std::get<Message>(result)), "Initial stream SUP"); }
            else { require(connection.observe(in, std::get<Message>(result)) ==
                           ProtocolError{SupportError::invalid_sequence}, "Pipelined SUP replay"); }
        };
        decoder.feed(std::string_view(wire).substr(0, split), sink);
        decoder.feed(std::string_view(wire).substr(split), sink);
        require(count == 2 && !decoder.finish() && !connection.support().peer_supports("TCP4") &&
                connection.support().peer_features() == std::vector<std::string>({"BASE", "TIGR"}), "Split invariance");
    }
}

// Isolated ncdc fixture bridge. Python owns sockets/TLS and reports completion;
// this is not a production Network adapter or an identity validation service.
void ncdc_transcript() {
    std::string initial;
    require(static_cast<bool>(std::getline(std::cin, initial)), "Missing client SUP");
    ProtocolConnection connection;
    accept(connection, in, initial);
    std::cout << *connection.support().initial_reply_line() << "ISID ABCD\n" << std::flush;
    std::string completion;
    require(static_cast<bool>(std::getline(std::cin, completion)) && completion == "SENT", "Missing send completion");
    require(!connection.observe(out, *connection.support().initial_reply()), "Committed ISUP");
    accept(connection, out, "ISID ABCD");
    std::string inf;
    require(static_cast<bool>(std::getline(std::cin, inf)), "Missing ncdc INF");
    const auto message = parse(inf);
    require(message.kind == Command::inf && message.type == MessageType::broadcast, "Expected ncdc BINF");
    require(!connection.observe(in, message), "ncdc INF rejected by state");
    for (const auto name : {"ID", "PD"}) {
        const auto field = std::find_if(message.parameters.begin(), message.parameters.end(), [name](const auto& token) {
            const auto parsed = parse_named_parameter(token);
            return parsed && parsed->name == name && parsed->value.size() == 39;
        });
        require(field != message.parameters.end(), "ncdc Tiger field shape");
    }
    require(connection.state() == ConnectionState::identify, "No implicit identity approval");
    connection.disconnect();
    std::cout << "IDENTIFY TIGR 24\n" << std::flush;
}
} // namespace

int main(int argc, char** argv) {
    static_assert(!std::is_copy_constructible_v<ProtocolConnection>);
    static_assert(!std::is_move_constructible_v<ProtocolConnection>);
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--ncdc-transcript") { ncdc_transcript(); return 0; }
        require(argc == 1, "Unexpected test arguments");
        negotiation_and_reply();
        failures_and_validation();
        updates_and_budgets();
        state_composition();
        every_stream_split();
        std::cout << "PASS: SUP negotiation, Tiger selection, atomic updates, budgets and state integration\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
