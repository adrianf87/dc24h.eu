#pragma once

#include "dc24h/i18n/messages.hpp"

#include <mutex>
#include <string_view>

namespace dc24h::logging {

// A shared logger serializes complete UTC lines to stderr (also captured by journald).
class Logger final {
public:
    explicit Logger(i18n::Language language = i18n::Language::us) noexcept : language_(language) {}
    void info(std::string_view message) noexcept;
    void error(std::string_view message) noexcept;

private:
    void write(const char* level, std::string_view message) noexcept;
    i18n::Language language_;
    std::mutex mutex_;
};

} // namespace dc24h::logging
