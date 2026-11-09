#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace dc24h::adc {

// Application limits, not limits imposed by the ADC specification. LF is excluded.
inline constexpr std::size_t max_frame_bytes = 16384;
inline constexpr std::size_t max_parameters = 128;

enum class MessageType : char {
    broadcast = 'B', client = 'C', direct = 'D', echo = 'E',
    feature = 'F', hub = 'H', info = 'I', udp = 'U'
};

enum class Command { unknown, sta, sup, sid, inf, msg, sch, res, ctm, rcm, gpa, pas, qui, get, gfi, snd };
[[nodiscard]] Command recognize_command(std::string_view name) noexcept;

struct FeatureSelector {
    bool required;
    std::string name;
};

struct Message {
    MessageType type;
    std::string command; // Unknown extension commands retain their original name.
    Command kind;
    std::optional<std::string> source; // SID, or CID for U
    std::optional<std::string> target;
    std::vector<FeatureSelector> features;
    // Unescaped values, in wire order. Named fields remain intact (e.g. "NIalice").
    std::vector<std::string> parameters;
};

struct KeepAlive {};
enum class ParseError {
    frame_too_long, too_many_parameters, invalid_utf8, non_normalized_text,
    control_character, invalid_header, invalid_separator, invalid_escape,
    invalid_sid, invalid_cid, invalid_features, truncated_frame
};
using ParseResult = std::variant<KeepAlive, Message, ParseError>;

// Exactly one frame body WITHOUT the terminating LF. Empty body is a keepalive.
// Validates line syntax, not command semantics, session state or authorization.
[[nodiscard]] ParseResult parse_line(std::string_view body);

// One decoder per TCP stream; confined to its owner's thread. No retained messages.
class StreamDecoder final {
public:
    using Sink = std::function<void(ParseResult)>;
    // Synchronous ordered delivery. Sink must not reenter this decoder; exceptions
    // propagate to the caller. Oversize input emits one error, discards until LF.
    void feed(std::string_view bytes, const Sink& sink);
    // Call on EOF: reports a partial frame once, resets and permits reuse.
    [[nodiscard]] std::optional<ParseError> finish() noexcept;
    // Includes oversized frames being discarded through their next LF.
    [[nodiscard]] bool in_progress() const noexcept { return discarding_ || !buffer_.empty(); }
    [[nodiscard]] std::size_t buffered_bytes() const noexcept { return buffer_.size(); }
private:
    std::string buffer_;
    bool discarding_{false};
};

} // namespace dc24h::adc
