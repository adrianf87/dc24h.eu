#pragma once

#include "dc24h/adc/protocol.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace dc24h::adc {

struct Limits {
    std::size_t frame_bytes = 16 * 1024; // Excludes the terminating LF.
    std::size_t parameters = 128;
    std::size_t parameter_bytes = 4 * 1024; // Counts escaped wire bytes.
};

enum class ParseError {
    incomplete_frame,
    multiple_frames,
    frame_too_long,
    invalid_header,
    invalid_sid,
    invalid_cid,
    invalid_feature,
    too_many_parameters,
    parameter_too_long,
    invalid_parameter,
    invalid_escape,
    invalid_utf8,
    non_nfc,
    text_processing_failed,
};

struct KeepAlive {};
using Frame = std::variant<KeepAlive, Message, ParseError>;
using ParameterResult = std::variant<std::string, ParseError>;

// Escape the wire delimiters in an already validated text value.
[[nodiscard]] std::string escape_parameter(std::string_view text);
[[nodiscard]] ParameterResult unescape_parameter(std::string_view token,
                                                 std::size_t max_bytes = Limits{}.parameter_bytes);
[[nodiscard]] Frame parse_frame(std::string_view wire, Limits limits = {});

struct DecodeResult {
    std::size_t consumed;
    std::optional<Frame> frame;
};

class FrameDecoder {
  public:
    explicit FrameDecoder(Limits limits = {});

    // Process up to one line or fragment. Resubmit any unconsumed bytes.
    [[nodiscard]] DecodeResult consume(std::string_view bytes);
    [[nodiscard]] std::size_t buffered_bytes() const noexcept;

    // Signal end of stream and reset the decoder.
    [[nodiscard]] std::optional<ParseError> finish() noexcept;

  private:
    Limits limits_;
    std::string buffer_;
    bool discarding_ = false;
};

} // namespace dc24h::adc
