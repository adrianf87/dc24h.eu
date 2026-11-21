#include "dc24h/adc/parser.hpp"

#include "wire.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <utf8proc.h>
#include <utility>

namespace dc24h::adc {
namespace {

std::optional<MessageType> message_type(char letter) {
    switch (letter) {
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'H':
    case 'I':
    case 'U':
        return static_cast<MessageType>(letter);
    default:
        return std::nullopt;
    }
}

std::string_view take_token(std::string_view &remaining) {
    const auto separator = remaining.find(' ');
    const auto token = remaining.substr(0, separator);
    remaining =
        separator == std::string_view::npos ? std::string_view{} : remaining.substr(separator + 1);
    return token;
}

std::optional<ParseError> parse_routing_fields(Message &message, std::string_view &remaining,
                                               Limits limits) {
    switch (message.type) {
    case MessageType::client:
    case MessageType::hub:
    case MessageType::information:
        return std::nullopt;
    case MessageType::udp: {
        const auto cid = take_token(remaining);
        if (cid.size() > limits.parameter_bytes) {
            return ParseError::parameter_too_long;
        }
        if (detail::base32_bytes(cid) == 0) {
            return ParseError::invalid_cid;
        }
        message.sender_cid = cid;
        return std::nullopt;
    }
    default:
        break;
    }

    const auto sid = take_token(remaining);
    if (!is_sid(sid)) {
        return ParseError::invalid_sid;
    }
    message.sender_sid = sid;

    if (message.type == MessageType::direct || message.type == MessageType::echo) {
        const auto target = take_token(remaining);
        if (!is_sid(target)) {
            return ParseError::invalid_sid;
        }
        message.target_sid = target;
    }

    if (message.type == MessageType::feature) {
        const auto filter = take_token(remaining);
        if (filter.size() > limits.parameter_bytes) {
            return ParseError::parameter_too_long;
        }
        constexpr std::size_t selector_width = 5;
        if (filter.empty() || filter.size() % selector_width != 0) {
            return ParseError::invalid_feature;
        }
        if (filter.size() / selector_width > limits.parameters) {
            return ParseError::too_many_parameters;
        }
        for (std::size_t offset = 0; offset < filter.size(); offset += selector_width) {
            const auto sign = filter[offset];
            const auto name = filter.substr(offset + 1, selector_width - 1);
            if ((sign != '+' && sign != '-') || !detail::wire_name(name, 4)) {
                return ParseError::invalid_feature;
            }
            message.selectors.push_back({std::string(name), sign == '+'});
        }
    }
    return std::nullopt;
}

std::optional<ParseError> validate_text(std::string_view text) {
    if (std::ranges::all_of(text,
                            [](char byte) { return static_cast<unsigned char>(byte) < 0x80U; })) {
        return std::nullopt;
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<utf8proc_ssize_t>::max())) {
        return ParseError::parameter_too_long;
    }

    utf8proc_uint8_t *normalized = nullptr;
    constexpr auto options = static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE);
    const auto length =
        utf8proc_map(reinterpret_cast<const utf8proc_uint8_t *>(text.data()),
                     static_cast<utf8proc_ssize_t>(text.size()), &normalized, options);
    const std::unique_ptr<utf8proc_uint8_t, decltype(&std::free)> storage(normalized, &std::free);
    if (length == UTF8PROC_ERROR_INVALIDUTF8) {
        return ParseError::invalid_utf8;
    }
    if (length < 0 || !storage) {
        return ParseError::text_processing_failed;
    }
    const std::string_view nfc(reinterpret_cast<const char *>(storage.get()),
                               static_cast<std::size_t>(length));
    if (text != nfc) {
        return ParseError::non_nfc;
    }
    return std::nullopt;
}

Frame parse_body(std::string_view body, Limits limits) {
    if (body.empty()) {
        return KeepAlive{};
    }
    if (body.back() == ' ') {
        return ParseError::invalid_parameter;
    }
    const auto header = take_token(body);
    if (header.size() != 4 || !detail::wire_name(header.substr(1), 3)) {
        return ParseError::invalid_header;
    }
    const auto type = message_type(header.front());
    if (!type) {
        return ParseError::invalid_header;
    }
    Message message;
    message.type = *type;
    message.command = header.substr(1);
    if (const auto error = parse_routing_fields(message, body, limits)) {
        return *error;
    }
    while (!body.empty()) {
        if (message.parameters.size() == limits.parameters) {
            return ParseError::too_many_parameters;
        }
        auto decoded = unescape_parameter(take_token(body), limits.parameter_bytes);
        if (const auto *error = std::get_if<ParseError>(&decoded)) {
            return *error;
        }
        message.parameters.push_back(std::get<std::string>(std::move(decoded)));
    }
    return message;
}

} // namespace

std::string escape_parameter(std::string_view text) {
    std::string output;
    output.reserve(text.size());
    for (char character : text) {
        switch (character) {
        case ' ':
            output += "\\s";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\\':
            output += "\\\\";
            break;
        default:
            output += character;
        }
    }
    return output;
}

ParameterResult unescape_parameter(std::string_view token, std::size_t max_bytes) {
    if (token.size() > max_bytes) {
        return ParseError::parameter_too_long;
    }
    if (token.empty() || token.find_first_of(" \n") != std::string_view::npos) {
        return ParseError::invalid_parameter;
    }
    std::string output;
    output.reserve(token.size());
    for (std::size_t index = 0; index < token.size(); ++index) {
        const auto character = token[index];
        if (character != '\\') {
            output += character;
            continue;
        }
        if (++index == token.size()) {
            return ParseError::invalid_escape;
        }
        switch (token[index]) {
        case 's':
            output += ' ';
            break;
        case 'n':
            output += '\n';
            break;
        case '\\':
            output += '\\';
            break;
        default:
            return ParseError::invalid_escape;
        }
    }
    if (const auto error = validate_text(output)) {
        return *error;
    }
    return output;
}

Frame parse_frame(std::string_view wire, Limits limits) {
    if (wire.empty() || wire.back() != '\n') {
        return ParseError::incomplete_frame;
    }
    wire.remove_suffix(1);
    if (wire.size() > limits.frame_bytes) {
        return ParseError::frame_too_long;
    }
    if (wire.find('\n') != std::string_view::npos) {
        return ParseError::multiple_frames;
    }
    return parse_body(wire, limits);
}

FrameDecoder::FrameDecoder(Limits limits) : limits_(limits) {}

DecodeResult FrameDecoder::consume(std::string_view bytes) {
    if (bytes.empty()) {
        return {0, std::nullopt};
    }
    const auto newline = bytes.find('\n');
    const bool terminated = newline != std::string_view::npos;
    const auto fragment = bytes.substr(0, newline);
    const auto consumed = fragment.size() + (terminated ? 1U : 0U);

    if (discarding_) {
        discarding_ = !terminated;
        return {consumed, std::nullopt};
    }
    if (fragment.size() > limits_.frame_bytes - buffer_.size()) {
        buffer_.clear();
        discarding_ = !terminated;
        return {consumed, ParseError::frame_too_long};
    }
    if (terminated && buffer_.empty()) {
        return {consumed, parse_body(fragment, limits_)};
    }
    buffer_.append(fragment);
    if (!terminated) {
        return {consumed, std::nullopt};
    }
    auto frame = parse_body(buffer_, limits_);
    buffer_.clear();
    return {consumed, std::move(frame)};
}

std::size_t FrameDecoder::buffered_bytes() const noexcept { return buffer_.size(); }

std::optional<ParseError> FrameDecoder::finish() noexcept {
    const bool incomplete = !buffer_.empty();
    buffer_.clear();
    discarding_ = false;
    if (incomplete) {
        return ParseError::incomplete_frame;
    }
    return std::nullopt;
}

} // namespace dc24h::adc
