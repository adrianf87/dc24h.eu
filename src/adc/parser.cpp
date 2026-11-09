#include "dc24h/adc/parser.hpp"

#include <utf8proc.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

namespace dc24h::adc {
namespace {
bool alpha(char c) noexcept { return c >= 'A' && c <= 'Z'; }
bool alnum(char c) noexcept { return alpha(c) || (c >= '0' && c <= '9'); }
bool base32(std::string_view value) noexcept {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](char c) {
        return alpha(c) || (c >= '2' && c <= '7');
    });
}
bool sid(std::string_view value) noexcept { return value.size() == 4 && base32(value); }
bool feature(std::string_view value) noexcept {
    return value.size() == 4 && alpha(value[0]) &&
           std::all_of(value.begin() + 1, value.end(), alnum);
}

std::optional<ParseError> validate_text(std::string_view text) {
    // Stricter local policy: reject raw ASCII controls, including CR/NUL/DEL.
    // Escaped LF is permitted in parameters and decoded after this check.
    for (const unsigned char c : text) {
        if (c < 0x20 || c == 0x7f) { return ParseError::control_character; }
    }
    utf8proc_uint8_t* output = nullptr;
    const auto length = utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(text.data()),
                                    static_cast<utf8proc_ssize_t>(text.size()), &output,
                                    static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE));
    const std::unique_ptr<utf8proc_uint8_t, decltype(&std::free)> normalized(output, &std::free);
    if (length == UTF8PROC_ERROR_NOMEM) { throw std::bad_alloc(); }
    if (length < 0) { return ParseError::invalid_utf8; }
    if (static_cast<std::size_t>(length) != text.size() ||
        !std::equal(text.begin(), text.end(), reinterpret_cast<const char*>(output))) {
        return ParseError::non_normalized_text;
    }
    return std::nullopt;
}

std::optional<std::string> unescape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\') { result.push_back(value[i]); continue; }
        if (++i == value.size()) { return std::nullopt; }
        switch (value[i]) {
        case 's': result.push_back(' '); break;
        case 'n': result.push_back('\n'); break;
        case '\\': result.push_back('\\'); break;
        default: return std::nullopt;
        }
    }
    return result;
}
} // namespace

Command recognize_command(std::string_view name) noexcept {
    constexpr std::array names{
        std::pair{"STA", Command::sta}, std::pair{"SUP", Command::sup},
        std::pair{"SID", Command::sid}, std::pair{"INF", Command::inf},
        std::pair{"MSG", Command::msg}, std::pair{"SCH", Command::sch},
        std::pair{"RES", Command::res}, std::pair{"CTM", Command::ctm},
        std::pair{"RCM", Command::rcm}, std::pair{"GPA", Command::gpa},
        std::pair{"PAS", Command::pas}, std::pair{"QUI", Command::qui},
        std::pair{"GET", Command::get}, std::pair{"GFI", Command::gfi},
        std::pair{"SND", Command::snd}
    };
    for (const auto& [text, kind] : names) { if (name == text) { return kind; } }
    return Command::unknown;
}

ParseResult parse_line(std::string_view body) {
    if (body.size() > max_frame_bytes) { return ParseError::frame_too_long; }
    if (body.empty()) { return KeepAlive{}; }
    if (const auto error = validate_text(body)) { return *error; }
    if (body.size() < 4 || !alpha(body[1]) || !alnum(body[2]) || !alnum(body[3])) {
        return ParseError::invalid_header;
    }
    switch (body[0]) {
    case 'B': case 'C': case 'D': case 'E': case 'F': case 'H': case 'I': case 'U': break;
    default: return ParseError::invalid_header;
    }
    if (body.size() > 4 && body[4] != ' ') { return ParseError::invalid_header; }

    std::vector<std::string_view> tokens;
    std::size_t offset = 4;
    while (offset < body.size()) {
        // Every iteration consumes the single separator before a nonempty token.
        ++offset;
        const auto next = body.find(' ', offset);
        const auto end = next == std::string_view::npos ? body.size() : next;
        if (end == offset) { return ParseError::invalid_separator; }
        if (tokens.size() == max_parameters + 2) { return ParseError::too_many_parameters; }
        tokens.push_back(body.substr(offset, end - offset));
        offset = end;
    }

    Message result{static_cast<MessageType>(body[0]), std::string(body.substr(1, 3)),
                   recognize_command(body.substr(1, 3)), {}, {}, {}, {}};
    std::size_t index = 0;
    if (result.type == MessageType::broadcast || result.type == MessageType::direct ||
        result.type == MessageType::echo || result.type == MessageType::feature) {
        if (index == tokens.size() || !sid(tokens[index])) { return ParseError::invalid_sid; }
        result.source = tokens[index++];
    } else if (result.type == MessageType::udp) {
        // CID length depends on negotiated hash, so syntax imposes no fixed length.
        if (index == tokens.size() || !base32(tokens[index])) { return ParseError::invalid_cid; }
        result.source = tokens[index++];
    }
    if (result.type == MessageType::direct || result.type == MessageType::echo) {
        if (index == tokens.size() || !sid(tokens[index])) { return ParseError::invalid_sid; }
        result.target = tokens[index++];
    }
    if (result.type == MessageType::feature) {
        if (index == tokens.size()) { return ParseError::invalid_features; }
        const auto selectors = tokens[index++];
        if (selectors.size() % 5 != 0) { return ParseError::invalid_features; }
        for (std::size_t i = 0; i < selectors.size(); i += 5) {
            if ((selectors[i] != '+' && selectors[i] != '-') || !feature(selectors.substr(i + 1, 4))) {
                return ParseError::invalid_features;
            }
            result.features.push_back({selectors[i] == '+', std::string(selectors.substr(i + 1, 4))});
        }
    }
    if (tokens.size() - index > max_parameters) { return ParseError::too_many_parameters; }
    result.parameters.reserve(tokens.size() - index);
    for (; index < tokens.size(); ++index) {
        auto value = unescape(tokens[index]);
        if (!value) { return ParseError::invalid_escape; }
        result.parameters.push_back(std::move(*value));
    }
    return result;
}

void StreamDecoder::feed(std::string_view bytes, const Sink& sink) {
    while (!bytes.empty()) {
        const auto lf = bytes.find('\n');
        const bool complete = lf != std::string_view::npos;
        const auto length = complete ? lf : bytes.size();
        if (!discarding_) {
            if (length > max_frame_bytes - buffer_.size()) {
                buffer_.clear();
                discarding_ = true;
                sink(ParseError::frame_too_long);
            } else {
                buffer_.append(bytes.data(), length);
                if (complete) {
                    auto result = parse_line(buffer_);
                    buffer_.clear();
                    sink(std::move(result));
                }
            }
        }
        if (!complete) { break; }
        discarding_ = false;
        bytes.remove_prefix(length + 1);
    }
}

std::optional<ParseError> StreamDecoder::finish() noexcept {
    const bool partial = !buffer_.empty() && !discarding_;
    buffer_.clear();
    discarding_ = false;
    return partial ? std::optional{ParseError::truncated_frame} : std::nullopt;
}

} // namespace dc24h::adc
