#include "dc24h/database/database.hpp"
#include "dc24h/database/migration.hpp"

#include <mysql.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
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
void option(MYSQL* handle, mysql_option key, const void* value) {
    if (mysql_optionsv(handle, key, value) != 0) {
        throw std::runtime_error("Database option setup failed");
    }
}
struct Statement {
    MYSQL_STMT* handle;
    Statement(MYSQL* connection, std::string_view sql, const std::vector<std::string>& parameters)
        : handle(mysql_stmt_init(connection)) {
        if (handle == nullptr) { throw std::runtime_error("Database statement failed"); }
        // Construction failures must close the partially prepared statement.
        try {
            if (sql.empty() || sql.size() > 65536 ||
                mysql_stmt_prepare(handle, sql.data(), static_cast<unsigned long>(sql.size())) != 0 ||
                mysql_stmt_param_count(handle) != parameters.size()) {
                throw std::runtime_error("Database statement failed");
            }
            std::vector<MYSQL_BIND> bindings(parameters.size());
            std::vector<unsigned long> lengths(parameters.size());
            for (std::size_t i = 0; i < parameters.size(); ++i) {
                if (parameters[i].size() > 65536) { throw std::runtime_error("Database statement failed"); }
                lengths[i] = static_cast<unsigned long>(parameters[i].size());
                bindings[i].buffer_type = MYSQL_TYPE_STRING;
                bindings[i].buffer = const_cast<char*>(parameters[i].data());
                bindings[i].buffer_length = lengths[i];
                bindings[i].length = &lengths[i];
            }
            if ((!bindings.empty() && mysql_stmt_bind_param(handle, bindings.data()) != 0) ||
                mysql_stmt_execute(handle) != 0) {
                throw std::runtime_error("Database statement failed");
            }
        } catch (...) { mysql_stmt_close(handle); throw; }
    }
    ~Statement() { mysql_stmt_close(handle); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
};
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
    const int fd = ::open(filename.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) { throw std::runtime_error("Cannot read private database password file"); }
    struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
    struct stat state{};
    if (::fstat(fd, &state) != 0 || !S_ISREG(state.st_mode) || state.st_uid != ::geteuid() ||
        (state.st_mode & 0077) != 0 || state.st_nlink != 1 || state.st_size < 1 || state.st_size > 4096) {
        throw std::runtime_error("Cannot read private database password file");
    }
    std::array<char, 4097> buffer{};
    struct Erase { std::array<char, 4097>& value; ~Erase() { OPENSSL_cleanse(value.data(), value.size()); } } erase{buffer};
    std::size_t size = 0;
    for (;;) {
        const auto count = ::read(fd, buffer.data() + size, buffer.size() - size);
        if (count < 0 && errno == EINTR) { continue; }
        if (count < 0) { throw std::runtime_error("Cannot read private database password file"); }
        if (count == 0) { break; }
        size += static_cast<std::size_t>(count);
        if (size > 4096) { throw std::runtime_error("Cannot read private database password file"); }
    }
    if (size > 0 && buffer[size - 1] == '\n') { --size; if (size > 0 && buffer[size - 1] == '\r') { --size; } }
    const std::string_view password(buffer.data(), size);
    if (!clean(password, 4096)) { throw std::runtime_error("Cannot read private database password file"); }
    return std::string(password);
}

struct Connection::Impl {
    MYSQL* handle{mysql_init(nullptr)};
    ~Impl() { if (handle != nullptr) { mysql_close(handle); } }
};

Connection::Connection(const Options& options) : impl_(std::make_unique<Impl>()) {
    validate(options);
    if (!options.enabled) { throw std::invalid_argument("Database is disabled"); }
    if (impl_->handle == nullptr) { throw std::runtime_error("Database initialization failed"); }
    auto* handle = impl_->handle;
    option(handle, MYSQL_OPT_CONNECT_TIMEOUT, &options.timeout);
    option(handle, MYSQL_OPT_READ_TIMEOUT, &options.timeout);
    option(handle, MYSQL_OPT_WRITE_TIMEOUT, &options.timeout);
    const my_bool off = 0;
    const unsigned int local_infile = 0;
    const unsigned int protocol = MYSQL_PROTOCOL_SOCKET;
    option(handle, MYSQL_OPT_PROTOCOL, &protocol);
    option(handle, MYSQL_OPT_RECONNECT, &off);
    // Local Unix socket security is provided by OS permissions, independently of TLS defaults.
    option(handle, MYSQL_OPT_SSL_ENFORCE, &off);
    option(handle, MYSQL_OPT_LOCAL_INFILE, &local_infile);
    option(handle, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    auto password = read_password(options.password_file);
    struct Erase { std::string& value; ~Erase() { OPENSSL_cleanse(value.data(), value.size()); } } erase{password};
    if (mysql_real_connect(handle, "localhost",
            options.user.c_str(), password.c_str(), options.name.c_str(), 0, options.socket.c_str(), 0) == nullptr) {
        throw std::runtime_error("Database connection failed");
    }
    if (mysql_set_character_set(handle, "utf8mb4") != 0) {
        throw std::runtime_error("Database character set setup failed");
    }
}
Connection::~Connection() = default;
void Connection::ping() {
    if (mysql_ping(impl_->handle) != 0) { throw std::runtime_error("Database ping failed"); }
}
void Connection::execute(std::string_view sql, const std::vector<std::string>& parameters) {
    const Statement statement(impl_->handle, sql, parameters);
    if (mysql_stmt_field_count(statement.handle) != 0) { throw std::runtime_error("Database result shape invalid"); }
}
std::optional<std::string> Connection::scalar(std::string_view sql, const std::vector<std::string>& parameters) {
    const Statement statement(impl_->handle, sql, parameters);
    if (mysql_stmt_field_count(statement.handle) != 1) { throw std::runtime_error("Database result shape invalid"); }
    std::array<char, 4096> buffer{};
    unsigned long length{};
    my_bool is_null{}, error{};
    MYSQL_BIND binding{};
    binding.buffer_type = MYSQL_TYPE_STRING;
    binding.buffer = buffer.data(); binding.buffer_length = buffer.size();
    binding.length = &length; binding.is_null = &is_null; binding.error = &error;
    if (mysql_stmt_bind_result(statement.handle, &binding) != 0 || mysql_stmt_fetch(statement.handle) != 0 || error) {
        throw std::runtime_error("Database result shape invalid");
    }
    const auto result = is_null ? std::nullopt : std::optional(std::string(buffer.data(), length));
    if (mysql_stmt_fetch(statement.handle) != MYSQL_NO_DATA) { throw std::runtime_error("Database result shape invalid"); }
    return result;
}

unsigned int migrate(Connection& connection) {
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
