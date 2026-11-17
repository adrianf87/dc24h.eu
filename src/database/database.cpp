#include "dc24h/database/database.hpp"
#include "dc24h/security/secret_file.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>

namespace dc24h::database {
Timeouts effective_timeouts(const Options& options) noexcept {
    return {options.connect_timeout ? options.connect_timeout : options.timeout,
        options.read_timeout ? options.read_timeout : options.timeout,
        options.write_timeout ? options.write_timeout : options.timeout};
}
namespace {
bool clean(std::string_view value, std::size_t maximum) {
    return !value.empty() && value.size() <= maximum &&
        std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
bool identifier(std::string_view value) {
    return !value.empty() && value.size() <= 64 &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        });
}
} // namespace

void validate(const Options& options) {
    if (!identifier(options.name) || !clean(options.user, 128) ||
        options.timeout == 0 || options.timeout > 60 || options.connect_timeout > 60 ||
        options.read_timeout > 60 || options.write_timeout > 60 || !clean(options.socket, 4096) ||
        (!options.password_file.empty() && !clean(options.password_file, 4096)) ||
        (options.enabled && options.password_file.empty())) {
        throw std::invalid_argument("Invalid database settings");
    }
}

std::string read_password(const std::string& filename) {
    try { return security::read_private_secret(filename); }
    catch (const std::bad_alloc&) { throw; }
    catch (const std::exception&) { throw std::runtime_error("Cannot read private database password file"); }
}

} // namespace dc24h::database
