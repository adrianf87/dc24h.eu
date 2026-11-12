#include "dc24h/search/features.hpp"

namespace dc24h::search {
std::optional<FeatureCode> FeatureCode::parse(std::string_view name) noexcept {
    if (name.size() != 4) { return {}; }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char ch = name[i];
        if (!((ch >= 'A' && ch <= 'Z') || (i == 3 && ch >= '0' && ch <= '9'))) { return {}; }
        value = (value << 8U) | static_cast<unsigned char>(ch);
    }
    return FeatureCode(value);
}
std::variant<FeatureFilter, FeatureError> FeatureFilter::create(const Selection& selection) {
    if (selection.required_features.size() > max_selectors ||
        selection.excluded_features.size() > max_selectors - selection.required_features.size()) {
        return FeatureError::too_many;
    }
    std::array<std::optional<FeatureCode>, max_selectors> rules{};
    std::size_t count = 0;
    const auto required = selection.required_features.size();
    for (const auto* names : {&selection.required_features, &selection.excluded_features}) {
        for (const auto& name : *names) {
            const auto code = FeatureCode::parse(name);
            if (!code) { return FeatureError::invalid_name; }
            for (std::size_t i = 0; i < count; ++i) {
                if (rules[i] == code) {
                    return i < required && count >= required ? FeatureError::conflict : FeatureError::duplicate;
                }
            }
            rules[count++] = code;
        }
    }
    // Validate before copying any untrusted local strings. No state is published
    // if the single owning Selection copy fails.
    return FeatureFilter(selection, rules, required, count);
}
bool FeatureFilter::matches(std::span<const std::string> advertised) const noexcept {
    if (count_ == 0) { return true; }
    std::uint64_t found = 0;
    for (const auto& name : advertised) {
        const auto code = FeatureCode::parse(name);
        if (!code) { continue; } // An unrelated label cannot satisfy a selector.
        for (std::size_t i = 0; i < count_; ++i) {
            if (rules_[i] != code) { continue; }
            if (i >= required_) { return false; }
            found |= std::uint64_t{1} << i;
            break;
        }
    }
    return found == (std::uint64_t{1} << required_) - 1;
}
} // namespace dc24h::search
