#include "dc24h/search/filters.hpp"
#include "dc24h/text/public_text.hpp"

#include <utf8proc.h>

#include <algorithm>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string_view>

namespace dc24h::search {
namespace {
// Called only after bounded UTF-8/NFC validation. Fold an internal comparison
// copy; neither the original criteria nor the candidate name are rewritten.
std::string fold(std::string_view value) {
    utf8proc_uint8_t* raw = nullptr;
    const auto length = utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(value.data()),
        static_cast<utf8proc_ssize_t>(value.size()), &raw,
        static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE | UTF8PROC_CASEFOLD));
    const std::unique_ptr<utf8proc_uint8_t, decltype(&std::free)> storage(raw, &std::free);
    if (length == UTF8PROC_ERROR_NOMEM) { throw std::bad_alloc(); }
    if (length < 0) { throw std::logic_error("Validated search text could not be folded"); }
    return {reinterpret_cast<const char*>(raw), static_cast<std::size_t>(length)};
}
} // namespace
struct Filter::Compiled {
    std::vector<std::string> include, exclude, extensions;
    std::optional<std::int64_t> min_bytes, max_bytes, exact_bytes;
    ItemType type{ItemType::any};
    std::optional<TthRoot> tth;
};
std::variant<Item, ItemError> Item::create(const ItemData& input) {
    if (input.type != ItemType::file && input.type != ItemType::directory) { return ItemError::invalid_type; }
    if (input.bytes && *input.bytes < 0) { return ItemError::invalid_size; }
    if (input.tth && input.type != ItemType::file) { return ItemError::invalid_hash; }
    if (input.name.empty() || input.name.size() > max_item_name_bytes ||
        std::all_of(input.name.begin(), input.name.end(), [](char c) { return c == ' '; }) ||
        (input.type == ItemType::file && input.name.back() == '/') || !text::valid_public_text(input.name)) {
        return ItemError::invalid_name;
    }
    return Item(input, fold(input.name));
}
std::variant<Filter, QueryError> Filter::create(const QueryData& input) {
    auto checked = Query::create(input);
    if (const auto* error = std::get_if<QueryError>(&checked)) { return *error; }
    const auto& data = std::get<Query>(checked).data();
    auto result = std::make_shared<Compiled>();
    result->min_bytes = data.min_bytes; result->max_bytes = data.max_bytes; result->exact_bytes = data.exact_bytes;
    result->type = data.type;
    result->tth = data.tth;
    for (const auto& term : data.include) { result->include.push_back(fold(term)); }
    for (const auto& term : data.exclude) { result->exclude.push_back(fold(term)); }
    for (const auto& extension : data.extensions) { result->extensions.push_back("." + fold(extension)); }
    return Filter(std::move(result));
}
bool Filter::matches(const Item& item) const noexcept {
    // Moved-from values cannot become an unrestricted predicate/candidate.
    if (!compiled_ || item.folded_.empty()) { return false; }
    const auto& filter = *compiled_;
    // Exact equality of all 192 bits. Missing hashes never stand in for zero;
    // extra name/extension/size/type criteria continue to constrain the match.
    if (filter.tth && (item.data_.type != ItemType::file || item.data_.tth != filter.tth)) { return false; }
    if (filter.type != ItemType::any && filter.type != item.data_.type) { return false; }
    if (filter.min_bytes || filter.max_bytes || filter.exact_bytes) {
        if (!item.data_.bytes) { return false; }
        const auto bytes = *item.data_.bytes;
        if ((filter.min_bytes && bytes < *filter.min_bytes) || (filter.max_bytes && bytes > *filter.max_bytes) ||
            (filter.exact_bytes && bytes != *filter.exact_bytes)) { return false; }
    }
    const std::string_view name = item.folded_;
    const auto contains = [&](const std::string& term) { return name.find(term) != std::string_view::npos; };
    if (!std::all_of(filter.include.begin(), filter.include.end(), contains) ||
        std::any_of(filter.exclude.begin(), filter.exclude.end(), contains)) { return false; }
    if (!filter.extensions.empty()) {
        if (item.data_.type != ItemType::file) { return false; }
        const auto last = name.find_last_of('/');
        const auto basename = name.substr(last == std::string_view::npos ? 0 : last + 1);
        // A dot boundary and a nonempty stem: EXiso never matches "notiso",
        // a parent directory or the bare hidden filename ".iso".
        if (!std::any_of(filter.extensions.begin(), filter.extensions.end(), [&](const std::string& suffix) {
            return basename.size() > suffix.size() && basename.ends_with(suffix);
        })) { return false; }
    }
    return true;
}
} // namespace dc24h::search
