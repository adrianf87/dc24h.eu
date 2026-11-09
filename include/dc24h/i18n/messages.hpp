#pragma once
#include <string>
#include <string_view>

namespace dc24h::i18n {
enum class Language { us, pl };
[[nodiscard]] Language parse_language(std::string_view name);
[[nodiscard]] std::string translate(std::string_view message, Language language);
[[nodiscard]] std::string_view help(Language language) noexcept;
} // namespace dc24h::i18n
