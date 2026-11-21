#include "dc24h/adc/protocol.hpp"
#include "dc24h/adc/parser.hpp"

#include "wire.hpp"

#include <array>
#include <cstdint>
#include <gcrypt.h>
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

std::optional<std::vector<Field>> parse_fields(std::span<const std::string> parameters) {
    std::vector<Field> fields;
    fields.reserve(parameters.size());
    for (const std::string_view parameter : parameters) {
        const auto name = parameter.substr(0, 2);
        if (!detail::wire_name(name, 2) || std::ranges::any_of(fields, [name](const Field &field) {
                return field.name == name;
            })) {
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

std::optional<std::string_view> parse_password_challenge(const Message &message) {
    const auto challenge = single_parameter(message, "GPA");
    return challenge && detail::base32_bytes(*challenge) >= 24 ? challenge : std::nullopt;
}

std::optional<std::string_view> parse_password_response(const Message &message) {
    const auto response = single_parameter(message, "PAS");
    return response && is_cid(*response) ? response : std::nullopt;
}

} // namespace dc24h::adc
