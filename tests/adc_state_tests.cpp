#include "dc24h/adc/state.hpp"

#include <array>
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
    require(std::holds_alternative<Message>(result), "Test fixture must parse");
    return std::get<Message>(std::move(result));
}
void accept(ConnectionStateMachine& machine, Direction direction, std::string_view text) {
    require(!machine.observe(direction, parse(text)), "Expected accepted message");
}
void reject(ConnectionStateMachine& machine, Direction direction, std::string_view text,
            StateError error) {
    const auto previous = machine.state();
    const std::string sid(machine.sid());
    require(machine.observe(direction, parse(text)) == error, "Unexpected rejection reason");
    require(machine.state() == previous && machine.sid() == sid, "Rejection changed state/SID");
}
void identify(ConnectionStateMachine& machine) {
    accept(machine, in, "HSUP ADBASE ADTIGR");
    require(!machine.approve_protocol(), "Validated negotiation must be approved");
    accept(machine, out, "ISUP ADBASE ADTIGR");
    require(machine.state() == ConnectionState::protocol, "ISUP alone must not advance");
    accept(machine, out, "ISID ABCD");
    require(machine.state() == ConnectionState::identify && machine.sid() == "ABCD", "SID transition");
}
void normal(ConnectionStateMachine& machine) {
    identify(machine);
    accept(machine, in, "BINF ABCD IDTEST PDprivate NIalice");
    require(!machine.approve_identity(IdentityMode::guest), "Guest approval");
}

void guest_and_password() {
    ConnectionStateMachine guest;
    identify(guest);
    accept(guest, out, "IINF CT32 NIdc24h.eu-twilight");
    reject(guest, out, "BINF ABCD NIalice", StateError::invalid_sequence);
    require(guest.approve_identity(IdentityMode::guest) == StateError::invalid_sequence,
            "Cannot approve without INF");
    accept(guest, in, "BINF ABCD IDTEST PDprivate NIalice");
    require(guest.state() == ConnectionState::identify, "Parsed INF is not validation");
    reject(guest, in, "BINF ABCD NIreplay", StateError::invalid_sequence);
    reject(guest, in, "BMSG ABCD premature", StateError::invalid_state);
    require(!guest.approve_identity(IdentityMode::guest), "Guest approval");
    require(guest.state() == ConnectionState::normal, "Guest must reach NORMAL");
    accept(guest, out, "BINF EFGH NIother");
    accept(guest, out, "BINF ABCD NIalice");
    accept(guest, in, "BINF ABCD DEupdated");
    accept(guest, in, "BMSG ABCD hello");
    require(guest.approve_identity(IdentityMode::guest) == StateError::invalid_state, "No approval replay");

    ConnectionStateMachine registered;
    identify(registered);
    accept(registered, in, "BINF ABCD NIregistered");
    require(!registered.approve_identity(IdentityMode::password_required), "Registered approval");
    require(registered.state() == ConnectionState::verify, "Registered must enter VERIFY");
    require(registered.approve_password() == StateError::invalid_sequence, "Cannot approve before PAS");
    reject(registered, in, "HPAS secret", StateError::invalid_sequence);
    accept(registered, out, "IGPA challenge");
    reject(registered, out, "IGPA replay", StateError::invalid_sequence);
    require(registered.approve_password() == StateError::invalid_sequence, "GPA is not PAS");
    accept(registered, in, "HPAS secret");
    require(registered.state() == ConnectionState::verify, "Parsed PAS is not authentication");
    reject(registered, in, "HPAS replay", StateError::invalid_sequence);
    reject(registered, in, "BMSG ABCD premature", StateError::invalid_state);
    require(!registered.approve_password(), "Validated password approval");
    require(registered.state() == ConnectionState::normal, "Password approval reaches NORMAL");
}

void handshake_order() {
    ConnectionStateMachine machine;
    require(machine.state() == ConnectionState::protocol && machine.sid().empty(), "Initial state");
    require(machine.approve_protocol() == StateError::invalid_sequence, "No SUP to approve");
    reject(machine, out, "ISUP ADBASE", StateError::invalid_sequence);
    reject(machine, out, "ISID ABCD", StateError::invalid_sequence);
    reject(machine, in, "BINF ABCD NIalice", StateError::invalid_state);
    reject(machine, in, "HPAS private", StateError::invalid_state);
    reject(machine, in, "ISID ABCD", StateError::invalid_type);
    accept(machine, in, "HSUP ADBASE ADTIGR");
    reject(machine, in, "HSUP ADBASE", StateError::invalid_sequence);
    reject(machine, out, "ISUP ADBASE", StateError::invalid_sequence);
    require(!machine.approve_protocol(), "Negotiation approval");
    require(machine.approve_protocol() == StateError::invalid_sequence, "No approval replay");
    reject(machine, out, "ISID ABCD", StateError::invalid_sequence);
    accept(machine, out, "ISUP ADBASE ADTIGR");
    reject(machine, out, "ISUP ADBASE", StateError::invalid_sequence);
    accept(machine, out, "ISID ABCD");
    reject(machine, out, "ISID EFGH", StateError::invalid_state);
    reject(machine, in, "HSUP ADBASE", StateError::invalid_state);
    require(machine.approve_protocol() == StateError::invalid_state, "No backwards approval");
    auto forged = parse("BINF ABCD NIalice");
    forged.command = "MSG";
    require(machine.observe(in, forged) == StateError::invalid_message, "Kind/name mismatch");
    forged = parse("ISID EFGH");
    forged.parameters.clear();
    ConnectionStateMachine separate;
    accept(separate, in, "HSUP ADBASE");
    require(!separate.approve_protocol(), "Separate protocol approval");
    accept(separate, out, "ISUP ADBASE");
    require(separate.observe(out, forged) == StateError::invalid_message, "Missing SID guard");
    forged.parameters = {"bad"};
    require(separate.observe(out, forged) == StateError::invalid_message, "Invalid SID guard");
    accept(separate, out, "ISID EFGH");
    require(machine.sid() == "ABCD" && separate.sid() == "EFGH", "Per-connection isolation");
}

// Independent command/state matrix. Check both directions and all BASE kinds;
// valid normal routing forms need no command execution or live network.
struct Fixture { const char* incoming; const char* outgoing; unsigned states; };
constexpr unsigned protocol = 1, identifying = 2, verifying = 4, active = 8;
constexpr std::array fixtures{
    Fixture{"HSTA 100 ok", "ISTA 100 ok", protocol | identifying | verifying | active},
    Fixture{"HSUP ADBASE", "ISUP ADBASE", protocol | active},
    Fixture{"HSID ABCD", "ISID ABCD", protocol},
    Fixture{"BINF ABCD NIalice", "IINF CT32", identifying | active},
    Fixture{"HMSG text", "IMSG text", active},
    Fixture{"BSCH ABCD ANfile", "BSCH EFGH ANfile", active},
    Fixture{"DRES ABCD EFGH FNfile", "DRES EFGH ABCD FNfile", active},
    Fixture{"DCTM ABCD EFGH ADC/1.0 1511 token", "DCTM EFGH ABCD ADC/1.0 1511 token", active},
    Fixture{"DRCM ABCD EFGH ADC/1.0 token", "DRCM EFGH ABCD ADC/1.0 token", active},
    Fixture{"HGPA challenge", "IGPA challenge", verifying},
    Fixture{"HPAS secret", "IPAS secret", verifying},
    Fixture{"HQUI EFGH", "IQUI EFGH", identifying | verifying | active},
    Fixture{"HGET file / 0 1", "IGET file / 0 1", 0},
    Fixture{"HGFI file /", "IGFI file /", 0},
    Fixture{"HSND file / 0 1", "ISND file / 0 1", 0},
    Fixture{"HXYZ opaque", "IXYZ opaque", 0}
};
void command_matrix() {
    for (const unsigned phase : {protocol, identifying, verifying, active}) {
        for (const auto direction : {in, out}) {
            for (const auto& fixture : fixtures) {
                ConnectionStateMachine machine;
                if (phase != protocol) { identify(machine); }
                if (phase == verifying || phase == active) {
                    accept(machine, in, "BINF ABCD NIalice");
                    require(!machine.approve_identity(phase == verifying ? IdentityMode::password_required :
                                                     IdentityMode::guest), "Matrix approval");
                }
                const auto message = parse(direction == in ? fixture.incoming : fixture.outgoing);
                // Supply handshake/challenge prerequisites, separately tested above.
                if (phase == protocol && direction == out &&
                    (message.kind == Command::sup || message.kind == Command::sid)) {
                    accept(machine, in, "HSUP ADBASE");
                    require(!machine.approve_protocol(), "Matrix protocol approval");
                    if (message.kind == Command::sid) { accept(machine, out, "ISUP ADBASE"); }
                }
                if (phase == verifying && direction == in && message.kind == Command::pas) {
                    accept(machine, out, "IGPA challenge");
                }
                const bool directional = !((direction == in && (message.kind == Command::sid ||
                    message.kind == Command::gpa || message.kind == Command::qui)) ||
                    (direction == out && message.kind == Command::pas));
                const auto old_state = machine.state();
                const bool allowed = directional && (fixture.states & phase) != 0;
                const auto result = machine.observe(direction, message);
                if (allowed != !result) {
                    throw std::runtime_error(std::string("Matrix mismatch: ") + fixture.incoming);
                }
                if (!allowed) { require(machine.state() == old_state, "Matrix rejection mutated state"); }
            }
        }
    }
}

void sources_and_transports() {
    ConnectionStateMachine machine;
    normal(machine);
    for (const char* text : {"BMSG EFGH spoof", "DMSG EFGH ABCD spoof", "EMSG EFGH ABCD spoof",
                             "FSCH EFGH +TCP4 ANfile", "BINF EFGH NIspoof"}) {
        reject(machine, in, text, StateError::wrong_source);
    }
    for (const char* text : {"BMSG ABCD hi", "DMSG ABCD EFGH hi", "EMSG ABCD EFGH hi",
                             "FSCH ABCD +TCP4 ANfile", "HSUP ADTCP4", "HSUP RMTCP4"}) {
        accept(machine, in, text);
    }
    for (const char* text : {"CMSG hi", "UMSG AAAA hi", "IMSG hi", "ISUP ADBASE", "IQUI ABCD"}) {
        reject(machine, in, text, StateError::invalid_type);
    }
    reject(machine, in, "HGET file / 0 8", StateError::unsupported_command);
    reject(machine, in, "HXYZ PDprivate", StateError::unsupported_command);
    // The syntax parser still preserves unknown extensions; policy does not execute them.
    require(parse("HXYZ PDprivate").kind == Command::unknown, "Opaque parser extensions preserved");
    accept(machine, out, "BMSG EFGH from_other_user");
    accept(machine, in, "ESTA ABCD EFGH 241 Unsupported");
    accept(machine, out, "DSTA EFGH ABCD 241 Unsupported");
    require(machine.state() == ConnectionState::normal, "Forwarded fatal STA must not close observer");
}

void closure_and_keepalive() {
    for (unsigned phase = 0; phase != 4; ++phase) {
        ConnectionStateMachine machine;
        if (phase > 0) { identify(machine); }
        if (phase > 1) {
            accept(machine, in, "BINF ABCD NIalice");
            require(!machine.approve_identity(phase == 2 ? IdentityMode::password_required :
                                             IdentityMode::guest), "Closure approval");
        }
        const auto previous = machine.state();
        require(!machine.keepalive() && machine.state() == previous, "Keepalive changed phase");
        accept(machine, in, "HSTA 100 recoverable");
        accept(machine, out, "ISTA 000 success");
        require(machine.state() == previous, "Status must not promote login");
        if (phase > 0) {
            accept(machine, out, "IQUI EFGH");
            require(machine.state() == previous, "Other-user QUI closed this client");
            accept(machine, out, "IQUI ABCD");
        } else { accept(machine, out, "ISTA 211 Hub\\sfull"); }
        require(machine.state() == ConnectionState::disconnected, "Terminal response");
        machine.disconnect();
        require(machine.keepalive() == StateError::disconnected, "Closed keepalive");
        require(machine.approve_protocol() == StateError::disconnected, "Closed protocol approval");
        require(machine.approve_identity(IdentityMode::guest) == StateError::disconnected, "Closed identity approval");
        require(machine.approve_password() == StateError::disconnected, "Closed password approval");
        for (const auto& fixture : fixtures) {
            reject(machine, in, fixture.incoming, StateError::disconnected);
            reject(machine, out, fixture.outgoing, StateError::disconnected);
        }
    }
    ConnectionStateMachine failed_auth;
    identify(failed_auth);
    accept(failed_auth, in, "BINF ABCD NIalice");
    require(!failed_auth.approve_identity(IdentityMode::password_required), "Failed auth setup");
    accept(failed_auth, out, "IGPA challenge");
    accept(failed_auth, in, "HPAS wrong");
    failed_auth.disconnect(); // Authentication rejection never invokes approval.
    require(failed_auth.approve_password() == StateError::disconnected, "Failed auth remains closed");
    ConnectionStateMachine eof;
    eof.disconnect();
    require(eof.state() == ConnectionState::disconnected, "EOF before login");
}

void stream_order() {
    // A pipelined early INF cannot skip the server's SUP/SID send boundary.
    ConnectionStateMachine machine;
    StreamDecoder decoder;
    unsigned accepted = 0, rejected = 0, invalid = 0;
    const auto sink = [&](ParseResult result) {
        if (const auto* message = std::get_if<Message>(&result)) {
            if (machine.observe(in, *message)) { ++rejected; } else { ++accepted; }
        } else if (std::holds_alternative<ParseError>(result)) { ++invalid; }
        else { require(!machine.keepalive(), "Live stream keepalive"); }
    };
    decoder.feed("HSUP ADBASE ADTIGR\nBINF ABCD NIearly\nHPAS secret\n\nHMSG\n", sink);
    require(accepted == 1 && rejected == 2 && invalid == 1, "Pipelined login bypass");
    require(machine.state() == ConnectionState::protocol, "No implicit transition");
    require(!machine.approve_protocol(), "Stream protocol approval");
    accept(machine, out, "ISUP ADBASE ADTIGR");
    accept(machine, out, "ISID ABCD");
    const std::string frame = "BINF ABCD NIzażółć PDprivate\n";
    for (const char byte : frame) { decoder.feed(std::string_view(&byte, 1), sink); }
    require(accepted == 2 && machine.state() == ConnectionState::identify, "Fragmented INF needs approval");
    require(!machine.approve_identity(IdentityMode::guest), "Stream identity approval");
    decoder.feed("BMSG EFGH spoof\nBMSG ABCD hello\n", sink);
    require(accepted == 3 && rejected == 3 && !decoder.finish(), "Stream source enforcement/recovery");
}
} // namespace

int main() {
    static_assert(!std::is_copy_constructible_v<ConnectionStateMachine>);
    static_assert(!std::is_move_constructible_v<ConnectionStateMachine>);
    try {
        guest_and_password();
        handshake_order();
        command_matrix();
        sources_and_transports();
        closure_and_keepalive();
        stream_order();
        std::cout << "PASS: hub state transitions, command matrix, approvals, sources, terminal and stream order\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
