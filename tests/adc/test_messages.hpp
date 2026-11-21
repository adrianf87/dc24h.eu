#pragma once

#include "dc24h/adc/parser.hpp"

namespace dc24h::adc::test {

// TIGR/192 vector for the byte sequence 0x00 through 0x17.
inline constexpr std::string_view pid = "AAAQEAYEAUDAOCAJBIFQYDIOB4IBCEQTCQKRMFY";
inline constexpr std::string_view cid = "W6AIUW3CLDF6OGHNVE4JPDDJ2P74IWRCF2O36TA";

inline Message decode(std::string_view wire) { return std::get<Message>(parse_frame(wire)); }

inline std::string identification(std::string_view client_id = cid,
                                  std::string_view private_id = pid) {
    std::string wire = "BINF ABCD ID";
    wire += client_id;
    wire += " PD";
    wire += private_id;
    wire += " NIGuest SUBASE\n";
    return wire;
}

} // namespace dc24h::adc::test
