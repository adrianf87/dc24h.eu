#pragma once

#include "dc24h/search/query.hpp"

#include <memory>
#include <utility>

namespace dc24h::search {
inline constexpr std::size_t max_item_name_bytes = 4096;
enum class ItemError { invalid_name, invalid_type, invalid_size, invalid_hash };
// Decoded virtual name including the share path, never an OS path to open.
// Unknown size is distinct from zero, including for directory descriptors.
struct ItemData {
    std::string name;
    ItemType type{ItemType::file};
    std::optional<std::int64_t> bytes;
    std::optional<TthRoot> tth{};
};
class Filter;
class Item final {
public:
    [[nodiscard]] static std::variant<Item, ItemError> create(const ItemData& input);
    [[nodiscard]] const ItemData& data() const noexcept { return data_; }
private:
    friend class Filter;
    Item(ItemData data, std::string folded) : data_(std::move(data)), folded_(std::move(folded)) {}
    ItemData data_;
    std::string folded_;
};
// Immutable compiled predicates over checked descriptors. No index, file access,
// wire parser or result registry. Creation can throw bad_alloc; matching cannot
// allocate, throw, log, mutate or retain the supplied Item.
class Filter final {
public:
    [[nodiscard]] static std::variant<Filter, QueryError> create(const QueryData& input);
    [[nodiscard]] bool matches(const Item& item) const noexcept;
private:
    struct Compiled;
    explicit Filter(std::shared_ptr<const Compiled> compiled) : compiled_(std::move(compiled)) {}
    std::shared_ptr<const Compiled> compiled_;
};
} // namespace dc24h::search
