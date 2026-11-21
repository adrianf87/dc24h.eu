#include "dc24h/adc/protocol.hpp"
#include "dc24h/adc/parser.hpp"

#include "wire.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <gcrypt.h>
#include <set>
#include <span>
#include <utility>

namespace dc24h::adc {
namespace {

using TigerValue = std::array<unsigned char, detail::tiger_bytes>;

// Called only for validated TIGR values.
TigerValue decode_tiger_value(std::string_view value) {
    TigerValue bytes{};
    std::uint32_t accumulator = 0;
    unsigned bits = 0;
    std::size_t offset = 0;
    for (const char symbol : value) {
        accumulator = (accumulator << 5U) | detail::base32_value(symbol);
        bits += 5U;
        if (bits >= 8U) {
            bits -= 8U;
            bytes[offset++] = static_cast<unsigned char>((accumulator >> bits) & 0xffU);
        }
    }
    return bytes;
}

std::optional<std::vector<Field>> parse_fields(std::span<const std::string> parameters,
                                               bool unique = true) {
    std::vector<Field> fields;
    fields.reserve(parameters.size());
    for (const std::string_view parameter : parameters) {
        const auto name = parameter.substr(0, 2);
        if (!detail::wire_name(name, 2) ||
            (unique && std::ranges::any_of(
                           fields, [name](const Field &field) { return field.name == name; }))) {
            return std::nullopt;
        }
        fields.push_back({name, parameter.substr(2)});
    }
    return fields;
}

std::optional<std::string_view> single_parameter(const Message &message, std::string_view command) {
    if (message.command != command || message.parameters.size() != 1) {
        return std::nullopt;
    }
    return message.parameters.front();
}

std::optional<std::uint64_t> decimal(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    std::uint64_t result = 0;
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end ? std::optional{result} : std::nullopt;
}

bool has_no_controls(std::string_view text) {
    return std::ranges::none_of(text, [](unsigned char byte) { return byte < 32 || byte == 127; });
}

bool valid_token(std::string_view value) {
    constexpr std::size_t max_token_bytes = 256;
    return !value.empty() && value.size() <= max_token_bytes && has_no_controls(value);
}

} // namespace

std::string Message::fourcc() const { return static_cast<char>(type) + command; }

std::string format_status(std::string_view code, std::string_view text, std::string_view field) {
    std::string line = "ISTA ";
    line += code;
    line += ' ';
    line += escape_parameter(text);
    if (!field.empty()) {
        line += ' ';
        line += escape_parameter(field);
    }
    line += '\n';
    return line;
}

std::string format_routing_header(const Message &message) {
    auto line = message.fourcc();
    if (message.sender_sid) {
        line += ' ';
        line += *message.sender_sid;
    }
    if (message.target_sid) {
        line += ' ';
        line += *message.target_sid;
    }
    if (message.type == MessageType::feature) {
        line += ' ';
        for (const auto &selector : message.selectors) {
            line += selector.required ? '+' : '-';
            line += selector.name;
        }
    }
    return line;
}

std::string format_routed(const Message &message) {
    auto line = format_routing_header(message);
    for (const auto &parameter : message.parameters) {
        line += ' ';
        line += escape_parameter(parameter);
    }
    line += '\n';
    return line;
}

bool is_sid(std::string_view value) noexcept {
    return value.size() == 4 && std::ranges::all_of(value, detail::base32_symbol);
}

bool is_cid(std::string_view value) noexcept {
    return value.size() == 39 && detail::base32_bytes(value) == detail::tiger_bytes;
}

bool is_feature(std::string_view value) noexcept {
    return value.size() == 4 && std::ranges::all_of(value.substr(0, 3), detail::uppercase) &&
           detail::alphanumeric(value.back());
}

bool is_keyprint(std::string_view value) noexcept {
    return value.starts_with("SHA256/") && detail::base32_bytes(value.substr(7)) == 32;
}

bool is_peer_command(std::string_view command) noexcept {
    return command == "SCH" || command == "RES" || command == "CTM" || command == "RCM";
}

IdentityCheck verify_tiger_identity(std::string_view cid, std::string_view pid) {
    if (!is_cid(cid)) {
        return IdentityCheck::invalid_cid;
    }
    if (!is_cid(pid)) {
        return IdentityCheck::invalid_pid;
    }
    static const bool initialized = gcry_check_version(nullptr) != nullptr;
    if (!initialized) {
        return IdentityCheck::hash_unavailable;
    }

    auto private_id = decode_tiger_value(pid);
    gcry_buffer_t input{};
    input.size = private_id.size();
    input.len = private_id.size();
    input.data = private_id.data();
    TigerValue digest{};
    // TIGER1 uses the digest byte order used in ADC and TTH.
    if (gcry_md_hash_buffers(GCRY_MD_TIGER1, 0, digest.data(), &input, 1) != 0) {
        return IdentityCheck::hash_unavailable;
    }
    return digest == decode_tiger_value(cid) ? IdentityCheck::valid : IdentityCheck::mismatch;
}

std::optional<std::vector<FeatureChange>> parse_support(const Message &message) {
    if (message.command != "SUP" || message.parameters.empty()) {
        return std::nullopt;
    }
    std::vector<FeatureChange> changes;
    changes.reserve(message.parameters.size());
    for (const std::string_view parameter : message.parameters) {
        if (parameter.size() != 6) {
            return std::nullopt;
        }
        const auto operation = parameter.substr(0, 2);
        const auto feature = parameter.substr(2);
        if ((operation != "AD" && operation != "RM") || !is_feature(feature)) {
            return std::nullopt;
        }
        changes.push_back({feature, operation == "AD"});
    }
    return changes;
}

std::optional<std::string_view> parse_sid(const Message &message) {
    const auto sid = single_parameter(message, "SID");
    return sid && is_sid(*sid) ? sid : std::nullopt;
}

std::optional<std::vector<Field>> parse_info(const Message &message) {
    if (message.command != "INF") {
        return std::nullopt;
    }
    return parse_fields(message.parameters);
}

std::optional<Status> parse_status(const Message &message) {
    if (message.command != "STA" || message.parameters.size() < 2) {
        return std::nullopt;
    }
    const auto &encoded = message.parameters[0];
    if (encoded.size() != 3 || !std::ranges::all_of(encoded, detail::digit) || encoded[0] > '2' ||
        (encoded[0] == '0' && encoded != "000") || message.parameters[1].empty()) {
        return std::nullopt;
    }
    auto fields = parse_fields(std::span(message.parameters).subspan(2));
    if (!fields) {
        return std::nullopt;
    }
    const auto severity = static_cast<Severity>(encoded[0] - '0');
    const auto code =
        static_cast<unsigned>(encoded[1] - '0') * 10U + static_cast<unsigned>(encoded[2] - '0');
    return Status{severity, code, message.parameters[1], std::move(*fields)};
}

std::optional<Quit> parse_quit(const Message &message) {
    if (message.command != "QUI" || message.parameters.empty() || !is_sid(message.parameters[0])) {
        return std::nullopt;
    }
    auto fields = parse_fields(std::span(message.parameters).subspan(1));
    if (!fields || std::ranges::any_of(*fields, [](const Field &field) {
            return field.name == "ID" && !is_sid(field.value);
        })) {
        return std::nullopt;
    }
    return Quit{message.parameters.front(), std::move(*fields)};
}

std::optional<Chat> parse_chat(const Message &message) {
    if (message.command != "MSG" || message.parameters.empty() ||
        message.parameters.front().empty()) {
        return std::nullopt;
    }
    const auto flags = parse_fields(std::span(message.parameters).subspan(1));
    if (!flags) {
        return std::nullopt;
    }
    Chat result{message.parameters.front(), std::nullopt};
    for (const auto &flag : *flags) {
        if (flag.name == "PM") {
            if (!is_sid(flag.value)) {
                return std::nullopt;
            }
            result.private_group = flag.value;
        } else if (flag.name == "ME") {
            if (flag.value != "1" && flag.value != "0") {
                return std::nullopt;
            }
            result.third_person = flag.value == "1";
        }
    }
    return result;
}

std::optional<std::string_view> parse_search(const Message &message) {
    if (message.command != "SCH") {
        return std::nullopt;
    }
    const auto fields = parse_fields(message.parameters, false);
    if (!fields) {
        return std::nullopt;
    }

    std::string_view token;
    std::optional<std::uint64_t> minimum;
    std::optional<std::uint64_t> maximum;
    std::optional<std::uint64_t> exact;
    std::set<std::string_view> singletons;
    bool has_filter = false;
    for (const auto &[name, value] : *fields) {
        if (!has_no_controls(value)) {
            return std::nullopt;
        }
        if (name == "AN" || name == "NO" || name == "EX") {
            if (value.empty() || (name == "EX" && value.starts_with('.'))) {
                return std::nullopt;
            }
            has_filter = true;
        } else if (name == "TO" || name == "TR" || name == "TY" || name == "GE" || name == "LE" ||
                   name == "EQ") {
            if (!singletons.insert(name).second) {
                return std::nullopt;
            }
            if (name == "TO") {
                if (!valid_token(value)) {
                    return std::nullopt;
                }
                token = value;
                continue;
            }
            if (name == "TR" || name == "TY") {
                if (name == "TR" ? !is_cid(value) : (value != "1" && value != "2")) {
                    return std::nullopt;
                }
            } else {
                const auto size = decimal(value);
                if (!size) {
                    return std::nullopt;
                }
                auto &bound = name == "GE" ? minimum : name == "LE" ? maximum : exact;
                bound = size;
            }
            has_filter = true;
        }
    }
    if (!has_filter || (minimum && maximum && *minimum > *maximum) ||
        (exact && ((minimum && *exact < *minimum) || (maximum && *exact > *maximum)))) {
        return std::nullopt;
    }
    return token;
}

std::optional<std::string_view> parse_search_result(const Message &message) {
    if (message.command != "RES") {
        return std::nullopt;
    }
    const auto fields = parse_fields(message.parameters);
    if (!fields) {
        return std::nullopt;
    }
    std::string_view path;
    std::string_view token;
    bool has_size = false;
    bool has_hash = false;
    for (const auto &[name, value] : *fields) {
        if (!has_no_controls(value)) {
            return std::nullopt;
        }
        if (name == "FN") {
            path = value;
        } else if (name == "SI" || name == "SL") {
            if (!decimal(value)) {
                return std::nullopt;
            }
            has_size |= name == "SI";
        } else if (name == "TR") {
            if (!is_cid(value)) {
                return std::nullopt;
            }
            has_hash = true;
        } else if (name == "TO") {
            if (!valid_token(value)) {
                return std::nullopt;
            }
            token = value;
        }
    }
    if (path.size() < 2 || !path.starts_with('/') || !has_size ||
        (!path.ends_with('/') && !has_hash)) {
        return std::nullopt;
    }
    return token;
}

std::optional<PeerConnection> parse_peer_connection(const Message &message) {
    const bool ctm = message.command == "CTM";
    if ((!ctm && message.command != "RCM") || message.parameters.size() != (ctm ? 3U : 2U)) {
        return std::nullopt;
    }
    const auto &protocol = message.parameters.front();
    const auto &token = message.parameters.back();
    if (protocol.empty() || protocol.size() > 64 || !has_no_controls(protocol) ||
        protocol.find(' ') != std::string::npos || !valid_token(token)) {
        return std::nullopt;
    }
    PeerConnection result{protocol, token, std::nullopt};
    if (ctm) {
        const auto port = decimal(message.parameters[1]);
        if (!port || *port == 0 || *port > 65535) {
            return std::nullopt;
        }
        result.port = static_cast<std::uint16_t>(*port);
    }
    return result;
}

std::optional<std::string_view> parse_password_challenge(const Message &message) {
    const auto challenge = single_parameter(message, "GPA");
    return challenge && detail::base32_bytes(*challenge) >= 24 ? challenge : std::nullopt;
}

std::optional<std::string_view> parse_password_response(const Message &message) {
    const auto response = single_parameter(message, "PAS");
    return response && is_cid(*response) ? response : std::nullopt;
}

} // namespace dc24h::adc
