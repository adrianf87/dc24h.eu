#pragma once

#include <cstddef>
#include <string_view>

namespace dc24h::text {
inline constexpr std::size_t max_public_text_bytes = 4096;
// Already-NFC UTF-8 only; no silent normalization. Reject control/format and
// line/paragraph separator characters. Nickname fragments also forbid ASCII
// space. Empty fragments are valid; callers enforce presence and field bounds.
// Bounded temporary normalization storage; allocation failure propagates.
[[nodiscard]] bool valid_public_text(std::string_view value, bool nickname = false);
} // namespace dc24h::text
