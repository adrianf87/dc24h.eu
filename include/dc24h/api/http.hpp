#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace dc24h::api {
inline constexpr std::size_t max_request_bytes = 8192;
inline constexpr std::size_t max_header_count = 32;

struct Request {
    std::string method;
    std::string target;
    std::string authorization;
    std::string body;
};
enum class ParseState { incomplete, complete, invalid, too_large };
struct ParseResult {
    ParseState state{ParseState::incomplete};
    Request request;
};

// Strict bodyless HTTP/1.1 subset: CRLF, one Host, no duplicate headers,
// no Transfer-Encoding/Expect/Upgrade, Content-Length absent or exactly 0.
// One request per connection. Already received trailing bytes are rejected.
// The separate maintenance server may opt into bounded ASCII form bodies on
// POST with exact Content-Length; the serving hub retains the bodyless subset.
[[nodiscard]] ParseResult parse_request(std::string_view bytes, bool allow_form = false);
[[nodiscard]] bool valid_token(std::string_view token) noexcept;
[[nodiscard]] bool authenticate(std::string_view authorization, std::string_view token) noexcept;
[[nodiscard]] std::string response(unsigned int status, std::string_view json, bool head = false);
} // namespace dc24h::api
