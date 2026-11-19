#include "dc24h/adc/protocol_control.hpp"
#include "dc24h/adc/serializer.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace dc24h::adc;

// Locked campaign. Both values are asserted below so a silent shrink fails.
constexpr std::uint32_t fuzz_seed = 0x0ADC0922u;
constexpr int mutation_count = 4096;
constexpr std::size_t mutation_cap = max_frame_bytes + 64;

void require(bool value, const std::string& reason) {
    if (!value) { throw std::runtime_error(reason); }
}

Message must_parse(std::string_view body) {
    auto result = parse_line(body);
    require(std::holds_alternative<Message>(result), "Fixture did not parse");
    return std::get<Message>(std::move(result));
}

bool same_message(const Message& left, const Message& right) {
    if (left.type != right.type || left.command != right.command || left.kind != right.kind ||
        left.source != right.source || left.target != right.target ||
        left.parameters != right.parameters || left.positional_count != right.positional_count ||
        left.features.size() != right.features.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.features.size(); ++i) {
        if (left.features[i].required != right.features[i].required ||
            left.features[i].name != right.features[i].name) {
            return false;
        }
    }
    return true;
}

bool same_result(const ParseResult& left, const ParseResult& right) {
    if (left.index() != right.index()) { return false; }
    if (std::holds_alternative<KeepAlive>(left)) { return true; }
    if (const auto* error = std::get_if<ParseError>(&left)) {
        return *error == std::get<ParseError>(right);
    }
    return same_message(std::get<Message>(left), std::get<Message>(right));
}

// Whole-buffer line split. An oversize frame without LF emits frame_too_long
// and finish() is empty, not truncated_frame. A short frame without LF is only
// truncated_frame. The decoder must match this, including across chunks.
void line_oracle(std::string_view wire, std::vector<ParseResult>& out, std::optional<ParseError>& tail) {
    std::size_t offset = 0;
    while (offset < wire.size()) {
        const auto lf = wire.find('\n', offset);
        if (lf == std::string_view::npos) {
            const auto length = wire.size() - offset;
            if (length > max_frame_bytes) {
                out.emplace_back(ParseError::frame_too_long);
                tail = std::nullopt;
            } else {
                tail = length == 0 ? std::nullopt : std::optional{ParseError::truncated_frame};
            }
            return;
        }
        const auto length = lf - offset;
        if (length > max_frame_bytes) {
            out.emplace_back(ParseError::frame_too_long);
        } else {
            out.push_back(parse_line(wire.substr(offset, length)));
        }
        offset = lf + 1;
    }
    tail = std::nullopt;
}

void round_trip(const Message& message, const std::string& label) {
    const auto serialized = serialize(message);
    require(std::holds_alternative<std::string>(serialized), label + " serialize failed");
    const auto& frame = std::get<std::string>(serialized);
    require(!frame.empty() && frame.back() == '\n', label + " frame has no LF");
    const auto again = parse_line(std::string_view(frame).substr(0, frame.size() - 1));
    require(std::holds_alternative<Message>(again), label + " reserialize did not parse");
    require(same_message(message, std::get<Message>(again)), label + " round-trip changed fields");
}

bool routed_status(const Message& message) noexcept {
    return message.kind == Command::sta && (message.type == MessageType::broadcast ||
        message.type == MessageType::direct || message.type == MessageType::echo ||
        message.type == MessageType::feature);
}

void agree(const Message& message, const std::string& label) {
    for (const auto direction : {Direction::from_client, Direction::to_client}) {
        ConnectionStateMachine machine;
        const auto code = control_message(direction, message);
        const auto observed = machine.observe(direction, message);
        if (code == ControlCode::unsupported_command) {
            require(observed == StateError::unsupported_command, label + " unsupported mismatch");
        } else if (code == ControlCode::invalid_type) {
            require(observed == StateError::invalid_type, label + " invalid_type mismatch");
        } else if (direction == Direction::to_client && routed_status(message)) {
            // A fresh machine is still in PROTOCOL. Routed STA stays invalid there.
            // That check remains in the state machine, after the profile.
            require(observed == StateError::invalid_type, label + " routed STA left PROTOCOL");
        } else {
            require(observed != StateError::unsupported_command && observed != StateError::invalid_type,
                    label + " profile accepted a command observe rejected as profile");
        }
    }
}

void feed_chunks(std::string_view wire, std::mt19937& rng, std::vector<ParseResult>& out,
                 std::optional<ParseError>& tail) {
    StreamDecoder decoder;
    const StreamDecoder::Sink sink = [&](ParseResult result) { out.push_back(std::move(result)); };
    for (std::size_t offset = 0; offset < wire.size();) {
        const auto step = static_cast<std::size_t>(rng() % 97U) + 1U;
        const auto length = std::min(step, wire.size() - offset);
        decoder.feed(wire.substr(offset, length), sink);
        require(decoder.buffered_bytes() <= max_frame_bytes, "Decoder buffer exceeded max_frame_bytes");
        offset += length;
    }
    tail = decoder.finish();
    require(decoder.buffered_bytes() == 0 && !decoder.in_progress(), "finish() left decoder state");
}

void check_wire(std::string_view wire, std::mt19937& chunks, const std::string& label) {
    std::vector<ParseResult> expected;
    std::vector<ParseResult> chunked;
    std::optional<ParseError> expected_tail;
    std::optional<ParseError> chunked_tail;
    line_oracle(wire, expected, expected_tail);
    feed_chunks(wire, chunks, chunked, chunked_tail);
    require(expected.size() == chunked.size() && expected_tail == chunked_tail, label + " oracle mismatch");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        require(same_result(expected[i], chunked[i]), label + " oracle result mismatch");
        if (const auto* message = std::get_if<Message>(&expected[i])) {
            round_trip(*message, label);
            agree(*message, label);
        }
    }
}

ControlCode expected_profile(Direction direction, MessageType type, Command kind) noexcept {
    const bool incoming = direction == Direction::from_client;
    switch (kind) {
    case Command::unknown:
    case Command::get:
    case Command::gfi:
    case Command::snd:
        return ControlCode::unsupported_command;
    case Command::pas:
        return incoming && type == MessageType::hub ? ControlCode::ok : ControlCode::invalid_type;
    case Command::sup:
        return type == (incoming ? MessageType::hub : MessageType::info)
            ? ControlCode::ok : ControlCode::invalid_type;
    case Command::sid:
    case Command::gpa:
    case Command::qui:
        return !incoming && type == MessageType::info ? ControlCode::ok : ControlCode::invalid_type;
    case Command::inf:
        if (incoming) {
            return type == MessageType::broadcast ? ControlCode::ok : ControlCode::invalid_type;
        }
        return type == MessageType::info || type == MessageType::broadcast
            ? ControlCode::ok : ControlCode::invalid_type;
    case Command::sta:
    case Command::msg: {
        const bool routed = type == MessageType::direct || type == MessageType::echo ||
            type == MessageType::broadcast || type == MessageType::feature;
        return routed || type == (incoming ? MessageType::hub : MessageType::info)
            ? ControlCode::ok : ControlCode::invalid_type;
    }
    case Command::sch:
    case Command::res:
    case Command::ctm:
    case Command::rcm:
        return type == MessageType::broadcast || type == MessageType::feature ||
            type == MessageType::direct || type == MessageType::echo
            ? ControlCode::ok : ControlCode::invalid_type;
    }
    return ControlCode::unsupported_command;
}

void profile_matrix() {
    constexpr std::array<MessageType, 8> types{
        MessageType::broadcast, MessageType::client, MessageType::direct, MessageType::echo,
        MessageType::feature, MessageType::hub, MessageType::info, MessageType::udp};
    constexpr std::array<Command, 16> commands{
        Command::unknown, Command::sta, Command::sup, Command::sid, Command::inf, Command::msg,
        Command::sch, Command::res, Command::ctm, Command::rcm, Command::gpa, Command::pas,
        Command::qui, Command::get, Command::gfi, Command::snd};
    int seen = 0;
    for (const auto type : types) {
        for (const auto kind : commands) {
            for (const auto direction : {Direction::from_client, Direction::to_client}) {
                Message message{type, "ZZZ", kind, std::nullopt, std::nullopt, {}, {}, std::nullopt};
                const auto code = control_message(direction, message);
                require(code == expected_profile(direction, type, kind), "Profile cell drifted");
                message.parameters.push_back(std::string(32, '\n'));
                require(control_message(direction, message) == code, "Profile inspected parameters");
                ++seen;
            }
        }
    }
    require(seen == 8 * 16 * 2, "Profile matrix changed size");
}

void kind_mismatch_precedes_profile() {
    auto message = must_parse("HSUP ADBASE");
    message.kind = Command::msg;
    ConnectionStateMachine machine;
    require(machine.observe(Direction::from_client, message) == StateError::invalid_message,
            "Kind/command mismatch was classified by the profile");
    require(machine.state() == ConnectionState::protocol, "Mismatch changed state");
}

void disconnected_precedes_profile() {
    const auto message = must_parse("HGET file / 0 1");
    require(control_message(Direction::from_client, message) == ControlCode::unsupported_command,
            "GET is not unsupported");
    ConnectionStateMachine machine;
    machine.disconnect();
    require(machine.observe(Direction::from_client, message) == StateError::disconnected,
            "Disconnected observe reached the profile");
}

void oversize_without_lf() {
    // One feed of an oversize body with no LF. check_wire compares the decoder,
    // including finish(), with the line-split oracle.
    const std::string wire(max_frame_bytes + 1, 'x');
    std::mt19937 chunks(fuzz_seed);
    check_wire(wire, chunks, "oversize without LF");
}

std::string mutate(std::mt19937& rng, std::string value) {
    const auto roll = rng() % 5U;
    if (value.empty() || roll == 1U) {
        if (value.size() < mutation_cap) {
            const auto index = value.empty() ? 0U : static_cast<std::size_t>(rng() % static_cast<std::uint32_t>(value.size()));
            value.insert(value.begin() + static_cast<std::ptrdiff_t>(index),
                         static_cast<char>(static_cast<unsigned char>(rng() % 256U)));
        }
    } else if (roll == 0U) {
        const auto index = static_cast<std::size_t>(rng() % static_cast<std::uint32_t>(value.size()));
        value[index] = static_cast<char>(static_cast<unsigned char>(rng() % 256U));
    } else if (roll == 2U) {
        const auto index = static_cast<std::size_t>(rng() % static_cast<std::uint32_t>(value.size()));
        value.erase(value.begin() + static_cast<std::ptrdiff_t>(index));
    } else if (roll == 3U) {
        const auto count = static_cast<std::size_t>(rng() % static_cast<std::uint32_t>(value.size())) + 1U;
        const auto room = mutation_cap - value.size();
        const auto copy = std::min(count, room);
        const std::string prefix(value.data(), copy);
        value.append(prefix);
    } else if (value.size() < mutation_cap) {
        value.push_back('\n');
    }
    if (value.size() > mutation_cap) { value.resize(mutation_cap); }
    return value;
}

void fuzz_campaign() {
    static_assert(fuzz_seed == 0x0ADC0922u);
    static_assert(mutation_count == 4096);
    constexpr std::array<std::string_view, 20> seeds{
        "HSUP ADBASE", "ISUP ADBASE ADTIGR", "ISID ABCD", "BINF ABCD NIalice", "HMSG text",
        "BMSG ABCD hello", "DMSG ABCD EFGH hello", "EMSG ABCD EFGH hello", "FSCH ABCD +TCP4 ANfile",
        "ISTA 100 ok", "IQUI ABCD", "HPAS secret", "IGPA challenge", "BSTA ABCD 100 ok",
        "BSCH ABCD ANfile", "DRES ABCD EFGH FNfile", "DCTM ABCD EFGH ADC/1.0 1511 token",
        "DRCM ABCD EFGH ADC/1.0 token", "HZ99 opaque", "HGET file / 0 1"};
    std::mt19937 generator(fuzz_seed);
    std::mt19937 chunks(fuzz_seed);
    for (const auto seed : seeds) {
        check_wire(seed, chunks, std::string(seed));
        check_wire(std::string(seed) + "\n", chunks, std::string(seed) + " LF");
    }
    int applied = 0;
    for (; applied < mutation_count; ++applied) {
        const auto seed = seeds[static_cast<std::size_t>(generator() % static_cast<std::uint32_t>(seeds.size()))];
        check_wire(mutate(generator, std::string(seed)), chunks, "mutation " + std::to_string(applied));
    }
    require(applied == 4096, "Fuzz campaign shrank");
}
} // namespace

int main() {
    try {
        profile_matrix();
        kind_mismatch_precedes_profile();
        disconnected_precedes_profile();
        oversize_without_lf();
        fuzz_campaign();
        std::cout << "PASS: ADC parser validation, 4096-mutation fuzz and protocol control\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "FAIL: " << failure.what() << '\n';
        return 1;
    }
}
