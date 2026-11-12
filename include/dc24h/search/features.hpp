#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace dc24h::search {
inline constexpr std::size_t max_selectors = 32;
struct Selection {
    std::vector<std::string> required_features, excluded_features;
    friend bool operator==(const Selection&, const Selection&) = default;
};
// Exact ADC feature identifiers: three uppercase letters and a final letter or
// digit. Unknown identifiers are valid; this is not a list of known extensions.
class FeatureCode final {
public:
    [[nodiscard]] static std::optional<FeatureCode> parse(std::string_view name) noexcept;
    friend bool operator==(const FeatureCode&, const FeatureCode&) = default;
private:
    explicit FeatureCode(std::uint32_t value) : value_(value) {}
    std::uint32_t value_;
};
enum class FeatureError { too_many, invalid_name, duplicate, conflict };
// Compiled once per plan, shared by preparation and dispatch. Public INF SU is
// supplied by the caller; negotiated SUP is deliberately not part of this API.
class FeatureFilter final {
public:
    [[nodiscard]] static std::variant<FeatureFilter, FeatureError> create(const Selection& selection);
    [[nodiscard]] const Selection& selection() const noexcept { return selection_; }
    [[nodiscard]] bool matches(std::span<const std::string> advertised) const noexcept;
private:
    FeatureFilter(Selection selection, std::array<std::optional<FeatureCode>, max_selectors> rules,
                  std::size_t required, std::size_t count)
        : selection_(std::move(selection)), rules_(rules), required_(required), count_(count) {}
    Selection selection_;
    std::array<std::optional<FeatureCode>, max_selectors> rules_{};
    std::size_t required_{}, count_{};
};
} // namespace dc24h::search
