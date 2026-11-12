#pragma once

#include "dc24h/adc/parser.hpp"
#include "dc24h/search/filters.hpp"

namespace dc24h::adc {
// Local decoded-payload bound; includes unknown fields and two-byte names.
inline constexpr std::size_t max_res_bytes = 8192;
enum class ResError {
    invalid_message, invalid_type, field_limit, duplicate_field, missing_field,
    invalid_name, invalid_integer, invalid_token, invalid_hash
};
// Initial hub profile: DRES only. Owns checked original fields and typed data.
// Virtual names are metadata, never filesystem paths. No routing or correlation.
class ResRequest final {
public:
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const search::ItemData& item() const noexcept { return item_; }
    [[nodiscard]] const std::optional<std::int64_t>& slots() const noexcept { return slots_; }
    [[nodiscard]] const std::optional<std::string>& token() const noexcept { return token_; }
private:
    friend std::variant<ResRequest, ResError> validate_res(const Message& message);
    friend std::variant<ResRequest, ParseError, ResError> parse_res(std::string_view body);
    ResRequest(Message message, search::ItemData item, std::optional<std::int64_t> slots,
               std::optional<std::string> token)
        : message_(std::move(message)), item_(std::move(item)), slots_(slots), token_(std::move(token)) {}
    Message message_;
    search::ItemData item_;
    std::optional<std::int64_t> slots_;
    std::optional<std::string> token_;
};
using ResParseResult = std::variant<ResRequest, ParseError, ResError>;
// Body excludes LF. Syntax errors remain ParseError. Source/state authorization
// belongs to the session layer; TO presence does not prove an active search.
[[nodiscard]] ResParseResult parse_res(std::string_view body);
[[nodiscard]] std::variant<ResRequest, ResError> validate_res(const Message& message);
} // namespace dc24h::adc
