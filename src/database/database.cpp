#include "dc24h/database/database.hpp"
#include "dc24h/database/migration.hpp"
#include "dc24h/security/secret_file.hpp"

#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <stdexcept>

namespace dc24h::database {
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
        options.timeout == 0 || options.timeout > 60 || !clean(options.socket, 4096) ||
        (!options.password_file.empty() && !clean(options.password_file, 4096)) ||
        (options.enabled && options.password_file.empty())) {
        throw std::invalid_argument("Invalid database settings");
    }
}

std::string read_password(const std::string& filename) {
    try { return security::read_private_secret(filename); }
    catch (const std::exception&) { throw std::runtime_error("Cannot read private database password file"); }
}

unsigned int migrate(Connection& connection) {
    if (connection.in_transaction()) { throw Error(ErrorKind::transaction, Operation::begin); }
    const auto name = connection.scalar("SELECT DATABASE()");
    if (!name) { throw std::runtime_error("Database migration failed"); }
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};
    SHA256(reinterpret_cast<const unsigned char*>(name->data()), name->size(), hash.data());
    std::string lock = "dc24h:";
    constexpr char hex[] = "0123456789abcdef";
    // 58-byte lock name, compatible with historical 64-byte name limits.
    for (std::size_t i = 0; i < 26; ++i) { lock += hex[hash[i] >> 4]; lock += hex[hash[i] & 15]; }
    if (connection.scalar("SELECT GET_LOCK(?, 0)", {lock}) != "1") {
        throw std::runtime_error("Database migration lock unavailable");
    }
    struct Unlock {
        Connection& connection; const std::string& lock;
        ~Unlock() { try { (void)connection.scalar("SELECT RELEASE_LOCK(?)", {lock}); } catch (...) {} }
    } unlock{connection, lock};
    connection.execute(baseline_sql);
    const auto count = connection.scalar("SELECT COUNT(*) FROM dc24h_schema_migrations");
    if (count == "0") {
        connection.execute("INSERT INTO dc24h_schema_migrations (version, name, checksum) VALUES (?, ?, ?)",
                           {"1", "schema_migrations", std::string(baseline_checksum)});
        return 1;
    }
    if (count != "1" || connection.scalar(
            "SELECT COUNT(*) FROM dc24h_schema_migrations WHERE version = ? AND name = ? AND checksum = ?",
            {"1", "schema_migrations", std::string(baseline_checksum)}) != "1") {
        throw std::runtime_error("Database migration history mismatch");
    }
    return 0;
}
} // namespace dc24h::database
