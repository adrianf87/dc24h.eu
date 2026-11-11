#pragma once

#include "dc24h/search/tth.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dc24h::search {
// Application bounds for textual/filter/TTH queries, not ADC wire limits.
inline constexpr std::size_t max_terms = 32;
inline constexpr std::size_t max_term_bytes = 256;
inline constexpr std::size_t max_extensions = 16;
inline constexpr std::size_t max_extension_bytes = 32;
inline constexpr std::size_t max_token_bytes = 128;
inline constexpr std::size_t max_query_bytes = 4096;
enum class ItemType { any, file, directory };
struct QueryData {
    std::vector<std::string> include, exclude, extensions;
    std::optional<std::int64_t> min_bytes, max_bytes, exact_bytes;
    ItemType type{ItemType::any};
    std::optional<std::string> token;
    std::optional<TthRoot> tth{};
    friend bool operator==(const QueryData&, const QueryData&) = default;
};
enum class QueryError { no_criteria, too_many_terms, invalid_text, invalid_extension,
                        invalid_size, invalid_type, invalid_token, too_large };
// Owning immutable criteria. No trimming, case folding, wildcard/regex execution,
// file access or protocol parsing. Allocation failure propagates to the caller.
class Query final {
public:
    [[nodiscard]] static std::variant<Query, QueryError> create(const QueryData& input);
    [[nodiscard]] const QueryData& data() const noexcept { return data_; }
private:
    explicit Query(const QueryData& input) : data_(input) {}
    QueryData data_;
};
} // namespace dc24h::search
