// Isolated libFuzzer entry point: no network, filesystem or identity material.
// Clang instrumented project libraries must be built with coverage enabled.
#include "dc24h/adc/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size > dc24h::adc::max_frame_bytes * 2) { return 0; }
    const auto bytes = std::string_view(reinterpret_cast<const char*>(data), size);
    // Exercise both bounded single-frame parsing and partial-stream handling.
    (void)dc24h::adc::parse_line(bytes);
    dc24h::adc::StreamDecoder decoder;
    const std::size_t split = size / 2;
    auto sink = [](dc24h::adc::ParseResult) {};
    decoder.feed(bytes.substr(0, split), sink);
    decoder.feed(bytes.substr(split), sink);
    (void)decoder.finish();
    return 0;
}
