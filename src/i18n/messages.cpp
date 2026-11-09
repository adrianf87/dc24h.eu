#include "dc24h/i18n/messages.hpp"
#include "us/messages.hpp"
#include "pl/messages.hpp"
#include <stdexcept>

namespace dc24h::i18n {
static_assert(us::messages.size() == pl::messages.size());
Language parse_language(std::string_view name) {
    if (name == "us") { return Language::us; }
    if (name == "pl") { return Language::pl; }
    throw std::invalid_argument("Unknown language (use us or pl)");
}
std::string translate(std::string_view message, Language language) {
    if (language == Language::pl) {
        for (std::size_t i = 0; i < us::messages.size(); ++i) {
            if (message.starts_with(us::messages[i])) {
                return std::string(pl::messages[i]) + std::string(message.substr(us::messages[i].size()));
            }
        }
    }
    return std::string(message);
}
std::string_view help(Language language) noexcept {
    return language == Language::pl ? pl::help : us::help;
}
} // namespace dc24h::i18n
