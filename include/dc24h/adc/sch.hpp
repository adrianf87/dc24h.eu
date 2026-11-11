#pragma once

#include "dc24h/adc/parser.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/search/core.hpp"
#include "dc24h/search/filters.hpp"

namespace dc24h::adc {
// Local decoded payload bound, including two-byte field names and unknown fields.
inline constexpr std::size_t max_sch_bytes = 4096;
enum class SchError {
    invalid_message, invalid_type, field_limit, duplicate_field, invalid_text,
    invalid_extension, invalid_integer, invalid_size, invalid_file_type,
    invalid_token, invalid_hash, invalid_features, no_criteria, unsupported_hash_query
};
// Owns both the original checked ADC fields (including unknown fields and numeric
// spellings) and their typed interpretation. No borrowed views survive a move.
class SchRequest final {
public:
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const search::QueryData& criteria() const noexcept { return criteria_; }
    [[nodiscard]] const search::Selection& selection() const noexcept { return selection_; }
    [[nodiscard]] const std::optional<TigerDigest>& root() const noexcept { return root_; }
    // Hash requests cannot be projected to textual filters: never discard TR.
    [[nodiscard]] std::variant<search::Query, search::QueryError, SchError> to_core_query() const;
    [[nodiscard]] std::variant<search::Filter, search::QueryError, SchError> to_filter() const;
private:
    friend std::variant<SchRequest, SchError> validate_sch(const Message& message);
    friend std::variant<SchRequest, ParseError, SchError> parse_sch(std::string_view body);
    SchRequest(Message message, search::QueryData criteria, search::Selection selection, std::optional<TigerDigest> root)
        : message_(std::move(message)), criteria_(std::move(criteria)), selection_(std::move(selection)), root_(root) {}
    Message message_;
    search::QueryData criteria_;
    search::Selection selection_;
    std::optional<TigerDigest> root_;
};
using SchParseResult = std::variant<SchRequest, ParseError, SchError>;
// Hub-side B/F/D/E profile. Body excludes LF; syntax errors retain ParseError.
// Typed validation is separate from session/source authorization and routing.
[[nodiscard]] SchParseResult parse_sch(std::string_view body);
// Also checks hostile hand-built Message objects against the shared wire parser.
// no_criteria means no recognized search criterion (including token-only input).
[[nodiscard]] std::variant<SchRequest, SchError> validate_sch(const Message& message);
} // namespace dc24h::adc
