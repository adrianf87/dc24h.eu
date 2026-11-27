// Separate bounded coverage target for fine-grained interleaving and stream
// completion. No network, credentials, persistent state or imported corpus.
#include "dc24h/adc/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size > 4096) { return 0; }
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    dc24h::adc::StreamDecoder decoder;
    std::size_t produced = 0;
    const auto sink = [&produced](dc24h::adc::ParseResult) { ++produced; };
    const std::size_t step = size ? (data[0] % 31) + 1 : 1;
    for (std::size_t at = 0; at < size;) {
        const std::size_t count = (size - at < step) ? (size - at) : step;
        decoder.feed(input.substr(at, count), sink);
        at += count;
    }
    (void)decoder.finish();
    (void)dc24h::adc::parse_line(input);
    return 0;
}
