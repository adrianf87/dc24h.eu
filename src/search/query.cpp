#include "dc24h/search/query.hpp"
#include "dc24h/text/public_text.hpp"

#include <algorithm>

namespace dc24h::search {
std::variant<Query, QueryError> Query::create(const QueryData& input) {
    if (input.include.empty() && input.exclude.empty() && input.extensions.empty() &&
        !input.min_bytes && !input.max_bytes && !input.exact_bytes && input.type == ItemType::any && !input.tth) {
        return QueryError::no_criteria;
    }
    if (input.include.size() > max_terms || input.exclude.size() > max_terms - input.include.size() ||
        input.extensions.size() > max_extensions) { return QueryError::too_many_terms; }
    if (input.type != ItemType::any && input.type != ItemType::file && input.type != ItemType::directory) {
        return QueryError::invalid_type;
    }
    for (const auto size : {input.min_bytes, input.max_bytes, input.exact_bytes}) {
        if (size && *size < 0) { return QueryError::invalid_size; }
    }
    if ((input.min_bytes && input.max_bytes && *input.min_bytes > *input.max_bytes) ||
        (input.exact_bytes && input.min_bytes && *input.exact_bytes < *input.min_bytes) ||
        (input.exact_bytes && input.max_bytes && *input.exact_bytes > *input.max_bytes)) {
        return QueryError::invalid_size;
    }
    std::size_t bytes = 0;
    const auto term_ok = [&](const std::string& term) {
        if (term.empty() || term.size() > max_term_bytes) { return false; }
        // An ASCII-space-only term must not become a match-all query.
        return std::any_of(term.begin(), term.end(), [](unsigned char c) { return c != ' '; });
    };
    for (const auto* terms : {&input.include, &input.exclude}) {
        for (const auto& term : *terms) {
            if (!term_ok(term)) { return QueryError::invalid_text; }
            bytes += term.size(); // Counts/individual sizes were bounded above.
        }
    }
    for (const auto& extension : input.extensions) {
        if (extension.empty() || extension.size() > max_extension_bytes || extension.front() == '.' ||
            extension.find_first_of("/\\ ") != std::string::npos) { return QueryError::invalid_extension; }
        bytes += extension.size();
    }
    if (input.token) {
        if (input.token->empty() || input.token->size() > max_token_bytes) {
            return QueryError::invalid_token;
        }
        bytes += input.token->size();
    }
    if (bytes > max_query_bytes) { return QueryError::too_large; }
    // Bound all strings before normalization/UTF-8 work or copying input.
    for (const auto* terms : {&input.include, &input.exclude}) {
        for (const auto& term : *terms) {
            if (!text::valid_public_text(term)) { return QueryError::invalid_text; }
        }
    }
    for (const auto& extension : input.extensions) {
        if (!text::valid_public_text(extension)) { return QueryError::invalid_extension; }
    }
    if (input.token && !text::valid_public_text(*input.token)) { return QueryError::invalid_token; }
    return Query(input);
}
} // namespace dc24h::search
