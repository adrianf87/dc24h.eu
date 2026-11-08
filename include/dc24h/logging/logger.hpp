#pragma once

#include <mutex>
#include <string_view>

namespace dc24h::logging {

// A shared logger serializes complete UTC lines to stderr (also captured by journald).
class Logger final {
public:
    void info(std::string_view message) noexcept;
    void error(std::string_view message) noexcept;

private:
    void write(const char* level, std::string_view message) noexcept;
    std::mutex mutex_;
};

} // namespace dc24h::logging
