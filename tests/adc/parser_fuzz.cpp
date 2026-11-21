#include "dc24h/adc/parser.hpp"
#include "dc24h/adc/session.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    using namespace dc24h::adc;

    const std::string_view input(reinterpret_cast<const char *>(data), size);
    static_cast<void>(parse_frame(input));

    const Limits limits;
    FrameDecoder decoder(limits);
    Session session("ABCD");
    auto remaining = input;
    while (!remaining.empty()) {
        const auto chunk = remaining.substr(0, 1U + static_cast<unsigned char>(remaining.front()));
        const auto result = decoder.consume(chunk);
        if (result.consumed == 0 || result.consumed > chunk.size() ||
            decoder.buffered_bytes() > limits.frame_bytes) {
            std::abort();
        }
        remaining.remove_prefix(result.consumed);
        if (!result.frame) {
            continue;
        }
        if (const auto *message = std::get_if<Message>(&*result.frame)) {
            static_cast<void>(parse_sid(*message));
            static_cast<void>(parse_quit(*message));
            static_cast<void>(parse_password_challenge(*message));
            const auto outcome = session.receive(*message);
            if (outcome.action == SessionAction::identified) {
                static_cast<void>(session.complete_identification());
            }
        }
    }
    static_cast<void>(decoder.finish());
    return 0;
}
