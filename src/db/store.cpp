#include "dc24h/db/store.hpp"

#include "dc24h/adc/parser.hpp"
#include "dc24h/net/config.hpp"

#include <boost/asio/ip/address.hpp>
#include <mysql.h>
#include <sys/un.h>

#include <algorithm>
#include <array>
#include <limits>

namespace dc24h::db {
namespace {

[[noreturn]] void fail(std::string_view operation, unsigned code) {
    throw Error("MariaDB " + std::string(operation) + " failed (code " + std::to_string(code) +
                ")");
}

void initialize_connector() {
    struct Library {
        Library() {
            if (mysql_library_init(0, nullptr, nullptr) != 0) {
                throw Error("Could not initialize MariaDB Connector/C");
            }
        }
        ~Library() { mysql_library_end(); }
    };
    struct Thread {
        Thread() {
            if (mysql_thread_init() != 0) {
                throw Error("Could not initialize the MariaDB client thread");
            }
        }
        ~Thread() { mysql_thread_end(); }
    };
    static const Library library;
    static thread_local const Thread thread;
}

class Connection {
  public:
    explicit Connection(const Config &config) : handle_(nullptr, &mysql_close) {
        const auto has_nul = [](const std::string &text) { return text.find('\0') != text.npos; };
        if (config.database.empty() || config.user.empty() || !config.socket.starts_with('/') ||
            config.socket.size() >= sizeof(sockaddr_un::sun_path) || has_nul(config.database) ||
            has_nul(config.user) || has_nul(config.password) || has_nul(config.socket)) {
            throw std::invalid_argument("Invalid MariaDB connection configuration");
        }
        initialize_connector();
        handle_.reset(mysql_init(nullptr));
        if (!handle_) {
            throw Error("Could not allocate a MariaDB connection");
        }
        const unsigned int protocol = MYSQL_PROTOCOL_SOCKET;
        const unsigned int local_infile = 0;
        const unsigned int timeout = 2;
        const my_bool reconnect = 0;
        option(MYSQL_OPT_PROTOCOL, &protocol);
        option(MYSQL_OPT_LOCAL_INFILE, &local_infile);
        option(MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
        option(MYSQL_OPT_READ_TIMEOUT, &timeout);
        option(MYSQL_OPT_WRITE_TIMEOUT, &timeout);
        option(MYSQL_OPT_RECONNECT, &reconnect);
        option(MYSQL_SET_CHARSET_NAME, "utf8mb4");
        if (!mysql_real_connect(get(), "localhost", config.user.c_str(), config.password.c_str(),
                                config.database.c_str(), 0, config.socket.c_str(), 0)) {
            fail("connection", mysql_errno(get()));
        }
        execute(
            "SET SESSION max_statement_time = 1, lock_wait_timeout = 1, "
            "innodb_lock_wait_timeout = 1, sql_mode = 'STRICT_ALL_TABLES,NO_ENGINE_SUBSTITUTION'",
            "session setup");
    }

    [[nodiscard]] MYSQL *get() const {
        if (!handle_) {
            throw Error("MariaDB connection is closed");
        }
        return handle_.get();
    }

    void discard() noexcept { handle_.reset(); }

    // Only internal, constant SQL without results is passed here.
    void execute(const char *sql, std::string_view operation) {
        if (mysql_query(get(), sql) != 0) {
            fail(operation, mysql_errno(get()));
        }
    }

  private:
    void option(mysql_option option, const void *value) {
        if (mysql_options(get(), option, value) != 0) {
            fail("client configuration", mysql_errno(get()));
        }
    }

    std::unique_ptr<MYSQL, decltype(&mysql_close)> handle_;
};

class Statement {
  public:
    Statement(Connection &connection, std::string_view sql, std::string_view operation)
        : handle_(mysql_stmt_init(connection.get()), &mysql_stmt_close), operation_(operation) {
        if (!handle_) {
            fail(operation_, mysql_errno(connection.get()));
        }
        check(
            mysql_stmt_prepare(handle_.get(), sql.data(), static_cast<unsigned long>(sql.size())));
    }

    template <class... Bindings> void execute(Bindings... parameters) {
        if (sizeof...(Bindings) != mysql_stmt_param_count(handle_.get())) {
            throw Error("Invalid MariaDB parameter count");
        }
        if constexpr (sizeof...(Bindings) > 0) {
            std::array bindings{parameters...};
            check(mysql_stmt_bind_param(handle_.get(), bindings.data()));
        }
        check(mysql_stmt_execute(handle_.get()));
    }

    template <class... Bindings> void results(Bindings... columns) {
        if (sizeof...(Bindings) != mysql_stmt_field_count(handle_.get())) {
            throw Error("Invalid MariaDB result column count");
        }
        std::array bindings{columns...};
        check(mysql_stmt_bind_result(handle_.get(), bindings.data()));
    }

    [[nodiscard]] bool next() {
        const auto result = mysql_stmt_fetch(handle_.get());
        if (result == MYSQL_NO_DATA) {
            return false;
        }
        check(result);
        return true;
    }

    [[nodiscard]] std::uint64_t inserted_id() const { return mysql_stmt_insert_id(handle_.get()); }

    [[nodiscard]] std::uint64_t affected_rows() const {
        return mysql_stmt_affected_rows(handle_.get());
    }

  private:
    void check(int result) const {
        if (result != 0) {
            fail(operation_, mysql_stmt_errno(handle_.get()));
        }
    }

    std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)> handle_;
    std::string_view operation_;
};

MYSQL_BIND parameter(std::string_view value) {
    MYSQL_BIND field{};
    field.buffer_type = MYSQL_TYPE_STRING;
    field.buffer = value.empty() ? const_cast<char *>("") : const_cast<char *>(value.data());
    field.buffer_length = static_cast<unsigned long>(value.size());
    return field;
}

MYSQL_BIND parameter(std::uint64_t &value) {
    MYSQL_BIND field{};
    field.buffer_type = MYSQL_TYPE_LONGLONG;
    field.buffer = &value;
    field.is_unsigned = 1;
    return field;
}

MYSQL_BIND sql_null() {
    MYSQL_BIND field{};
    field.buffer_type = MYSQL_TYPE_NULL;
    return field;
}

MYSQL_BIND bind_nullable(std::string_view value) {
    return value.empty() ? sql_null() : parameter(value);
}

struct UnsignedResult {
    std::uint64_t value = 0;
    my_bool is_null = 0;

    [[nodiscard]] MYSQL_BIND column() {
        auto field = parameter(value);
        field.is_null = &is_null;
        return field;
    }
};

template <std::size_t Size> struct TextResult {
    std::array<char, Size> buffer{};
    unsigned long length = 0;
    my_bool is_null = 0;

    [[nodiscard]] MYSQL_BIND column() {
        MYSQL_BIND field{};
        field.buffer_type = MYSQL_TYPE_STRING;
        field.buffer = buffer.data();
        field.buffer_length = static_cast<unsigned long>(buffer.size());
        field.length = &length;
        field.is_null = &is_null;
        return field;
    }

    [[nodiscard]] std::string text() const {
        if (is_null || length > buffer.size()) {
            throw Error("Invalid MariaDB text result");
        }
        return {buffer.data(), length};
    }

    [[nodiscard]] std::optional<std::string> nullable() const {
        return is_null ? std::nullopt : std::optional{text()};
    }
};

class Transaction {
  public:
    explicit Transaction(Connection &connection)
        : connection_(connection), handle_(connection.get()) {
        connection_.execute("START TRANSACTION", "begin transaction");
    }
    ~Transaction() {
        if (!committed_ && mysql_rollback(handle_) != 0) {
            connection_.discard();
        }
    }
    void commit() {
        if (mysql_commit(handle_) != 0) {
            fail("commit", mysql_errno(handle_));
        }
        committed_ = true;
    }

  private:
    Connection &connection_;
    MYSQL *handle_;
    bool committed_ = false;
};

constexpr char migration_history[] = R"sql(
    CREATE TABLE IF NOT EXISTS schema_migrations (
        version SMALLINT UNSIGNED NOT NULL PRIMARY KEY,
        name VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
        applied_at DATETIME(6) NOT NULL DEFAULT (UTC_TIMESTAMP(6))
    ) ENGINE=InnoDB
)sql";

struct Migration {
    const char *table;
    const char *ddl;
    const char *probe;
};

constexpr Migration migrations[] = {
    {"hub_settings", R"sql(
        CREATE TABLE IF NOT EXISTS hub_settings (
            name VARCHAR(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
            value VARCHAR(255) NOT NULL,
            updated_at DATETIME(6) NOT NULL DEFAULT (UTC_TIMESTAMP(6))
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )sql",
     "SELECT name, value, updated_at FROM hub_settings LIMIT 0"},
    {"bans", R"sql(
        CREATE TABLE IF NOT EXISTS bans (
            id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
            kind ENUM('cid', 'ip') CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
            identifier VARCHAR(45) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
            reason VARCHAR(512) NOT NULL,
            created_at DATETIME(6) NOT NULL DEFAULT (UTC_TIMESTAMP(6)),
            expires_at DATETIME(6) NULL,
            actor VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
            INDEX ban_lookup (kind, identifier, expires_at)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )sql",
     "SELECT id, kind, identifier, reason, created_at, expires_at, actor FROM bans LIMIT 0"},
    {"audit_events", R"sql(
        CREATE TABLE IF NOT EXISTS audit_events (
            id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
            event_type VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
            created_at DATETIME(6) NOT NULL DEFAULT (UTC_TIMESTAMP(6)),
            actor VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NULL,
            object_id VARCHAR(48) CHARACTER SET ascii COLLATE ascii_bin NULL,
            details VARCHAR(96) CHARACTER SET ascii COLLATE ascii_bin NOT NULL
        ) ENGINE=InnoDB
    )sql",
     "SELECT id, event_type, created_at, actor, object_id, details FROM audit_events LIMIT 0"},
    {"admin_users", R"sql(
        CREATE TABLE IF NOT EXISTS admin_users (
            id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(48) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
            password_hash VARCHAR(160) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
            role ENUM('admin', 'viewer') CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
            created_at DATETIME(6) NOT NULL DEFAULT (UTC_TIMESTAMP(6))
        ) ENGINE=InnoDB
    )sql",
     "SELECT id, username, password_hash, role, created_at FROM admin_users LIMIT 0"},
    {"admin_sessions", R"sql(
        CREATE TABLE IF NOT EXISTS admin_sessions (
            token_hash BINARY(32) NOT NULL PRIMARY KEY,
            admin_user_id BIGINT UNSIGNED NOT NULL,
            created_at DATETIME(6) NOT NULL DEFAULT (UTC_TIMESTAMP(6)),
            expires_at DATETIME(6) NOT NULL,
            INDEX admin_sessions_expiry (expires_at),
            INDEX admin_sessions_user (admin_user_id),
            FOREIGN KEY (admin_user_id) REFERENCES admin_users (id) ON DELETE CASCADE
        ) ENGINE=InnoDB
    )sql",
     "SELECT token_hash, admin_user_id, created_at, expires_at FROM admin_sessions LIMIT 0"},
};
static_assert(std::size(migrations) == current_schema);

void verify_table(Connection &connection, std::string_view table, std::string_view probe) {
    {
        UnsignedResult exists;
        Statement engine(
            connection,
            "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() "
            "AND TABLE_NAME = ? AND TABLE_TYPE = 'BASE TABLE' AND ENGINE = 'InnoDB'",
            "verify migration table");
        engine.execute(parameter(table));
        engine.results(exists.column());
        if (!engine.next() || exists.value != 1) {
            throw Error("MariaDB migration requires an InnoDB table for " + std::string(table));
        }
    }
    Statement columns(connection, probe, table);
}

unsigned read_version(Connection &connection) {
    UnsignedResult version;
    TextResult<64> name;
    Statement query(connection, "SELECT version, name FROM schema_migrations ORDER BY version",
                    "read migration history");
    query.execute();
    query.results(version.column(), name.column());
    unsigned applied = 0;
    while (query.next()) {
        if (applied >= current_schema || version.value != applied + 1 ||
            name.text() != migrations[applied].table) {
            throw Error("MariaDB schema history is unknown or inconsistent");
        }
        ++applied;
    }
    return applied;
}

bool valid_actor(std::string_view actor) {
    return actor.size() <= 64 && std::ranges::all_of(actor, [](unsigned char byte) {
               return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
                      (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' ||
                      byte == '@';
           });
}

std::string canonical_ip(std::string_view value) {
    if (value.empty() || value.size() > 45 || value.find('%') != value.npos ||
        value.find('\0') != value.npos) {
        throw std::invalid_argument("Invalid IPv4 or IPv6 address");
    }
    namespace ip = boost::asio::ip;
    boost::system::error_code error;
    const auto address = ip::make_address(std::string(value), error);
    if (error) {
        throw std::invalid_argument("Invalid IPv4 or IPv6 address");
    }
    if (address.is_v6() && address.to_v6().is_v4_mapped()) {
        return ip::make_address_v4(ip::v4_mapped, address.to_v6()).to_string();
    }
    return address.to_string();
}

std::string canonical_cid(std::string_view value) {
    if (!adc::is_cid(value)) {
        throw std::invalid_argument("Invalid ADC CID");
    }
    return std::string(value);
}

bool valid_reason(std::string_view reason) {
    return !reason.empty() && reason.size() <= 512 &&
           std::ranges::none_of(reason,
                                [](unsigned char byte) { return byte < 32 || byte == 127; }) &&
           std::holds_alternative<std::string>(
               adc::unescape_parameter(adc::escape_parameter(reason)));
}

AdminRole parse_role(std::string_view role) {
    if (role == "admin") {
        return AdminRole::admin;
    }
    if (role == "viewer") {
        return AdminRole::viewer;
    }
    throw Error("Invalid administrator role in MariaDB");
}

void validate_digest(std::string_view digest) {
    if (digest.size() != 64 || !std::ranges::all_of(digest, [](char byte) {
            return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
        })) {
        throw std::invalid_argument("Invalid administrator session digest");
    }
}

void validate_page_size(std::uint64_t limit) {
    // The API can read one additional row to check whether the page has a successor.
    if (limit == 0 || limit > 101) {
        throw std::invalid_argument("Invalid page size");
    }
}

} // namespace

bool valid_admin_username(std::string_view name) noexcept {
    return name.size() >= 3 && name.size() <= 48 && std::ranges::all_of(name, [](char byte) {
               return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') || byte == '-' ||
                      byte == '_' || byte == '.';
           });
}

std::string_view role_name(AdminRole role) noexcept {
    return role == AdminRole::admin ? "admin" : "viewer";
}

class Store::Impl {
  public:
    explicit Impl(const Config &config) : connection(config) {}

    void audit(std::string_view type, std::string_view actor, std::string_view object,
               std::string_view details) {
        Statement insert(connection,
                         "INSERT INTO audit_events (event_type, actor, object_id, details) "
                         "VALUES (?, ?, ?, ?)",
                         "write audit event");
        insert.execute(parameter(type), bind_nullable(actor), bind_nullable(object),
                       parameter(details));
    }

    template <class Operation>
    auto administrative(std::string_view action, std::string_view actor, std::string_view object,
                        Operation operation) {
        const bool actor_ok = valid_actor(actor);
        try {
            if (!actor_ok) {
                throw std::invalid_argument("Invalid administrator identifier");
            }
            Transaction transaction(connection);
            auto result = operation();
            transaction.commit();
            return result;
        } catch (const std::invalid_argument &) {
            failure(action, actor_ok ? actor : std::string_view{}, object, "invalid_input");
            throw;
        } catch (const Error &) {
            failure(action, actor, object, "database_error");
            throw;
        }
    }

    Connection connection;

  private:
    void failure(std::string_view action, std::string_view actor, std::string_view object,
                 std::string_view reason) {
        // A failed database operation may also prevent recording its failure.
        try {
            audit("admin.failed", actor, object, std::string(action) + ' ' + std::string(reason));
        } catch (const Error &) {
        }
    }
};

unsigned migrate(const Config &config) {
    Connection connection(config);
    {
        UnsignedResult acquired;
        Statement lock(
            connection,
            "SELECT GET_LOCK(CONCAT('dc24h:', LEFT(SHA2(LOWER(DATABASE()), 256), 56)), 1)",
            "acquire migration lock");
        lock.execute();
        lock.results(acquired.column());
        if (!lock.next() || acquired.is_null || acquired.value != 1) {
            throw Error("Could not acquire the MariaDB migration lock");
        }
    }
    connection.execute(migration_history, "create migration history");
    verify_table(connection, "schema_migrations",
                 "SELECT version, name, applied_at FROM schema_migrations LIMIT 0");
    auto version = read_version(connection);
    for (unsigned index = 0; index < current_schema; ++index) {
        const auto &migration = migrations[index];
        if (index >= version) {
            connection.execute(migration.ddl, migration.table);
        }
        verify_table(connection, migration.table, migration.probe);
        if (index >= version) {
            // DDL commits implicitly. Record a step only after its table is usable.
            std::uint64_t applied = index + 1;
            Statement mark(connection,
                           "INSERT INTO schema_migrations (version, name) VALUES (?, ?)",
                           "record migration");
            mark.execute(parameter(applied), parameter(migration.table));
            ++version;
        }
    }
    return version;
}

Store::Store(const Config &config) : impl_(std::make_unique<Impl>(config)) {}
Store::~Store() = default;

unsigned Store::schema_version() { return read_version(impl_->connection); }

void Store::require_current_schema() {
    if (schema_version() != current_schema) {
        throw Error("MariaDB schema requires migration");
    }
}

Settings Store::settings() {
    TextResult<48> name;
    TextResult<255 * 4> value;
    Statement query(impl_->connection,
                    "SELECT name, value FROM hub_settings ORDER BY name LIMIT 65", "read settings");
    query.execute();
    query.results(name.column(), value.column());
    Settings result;
    while (query.next()) {
        if (result.size() == 64) {
            throw Error("Too many MariaDB hub settings");
        }
        result.emplace_back(name.text(), value.text());
    }
    return result;
}

void Store::set_setting(std::string_view name, std::string_view value, std::string_view actor) {
    impl_->administrative("settings.set", actor, {}, [&] {
        net::ServerConfig config;
        if (!net::apply_hub_setting(config, name, value)) {
            throw std::invalid_argument("Unknown hub setting");
        }
        Statement update(impl_->connection,
                         "INSERT INTO hub_settings (name, value) VALUES (?, ?) "
                         "ON DUPLICATE KEY UPDATE value = VALUES(value), "
                         "updated_at = UTC_TIMESTAMP(6)",
                         "write setting");
        update.execute(parameter(name), parameter(value));
        impl_->audit("setting.updated", actor, name, "value saved");
        return true;
    });
}

std::uint64_t Store::add_ban(const BanInput &input, std::string_view actor) {
    return impl_->administrative("bans.add", actor, {}, [&] {
        if (!valid_reason(input.reason) || input.duration_seconds == 0) {
            throw std::invalid_argument("Invalid ban reason or duration");
        }
        const auto identifier = input.kind == BanKind::cid ? canonical_cid(input.identifier)
                                                           : canonical_ip(input.identifier);
        const std::string_view kind = input.kind == BanKind::cid ? "cid" : "ip";
        std::uint64_t duration = input.duration_seconds.value_or(0);
        Statement insert(impl_->connection,
                         "INSERT INTO bans (kind, identifier, reason, expires_at, actor) "
                         "VALUES (?, ?, ?, TIMESTAMPADD(SECOND, ?, UTC_TIMESTAMP(6)), ?)",
                         "add ban");
        insert.execute(parameter(kind), parameter(identifier), parameter(input.reason),
                       input.duration_seconds ? parameter(duration) : sql_null(),
                       bind_nullable(actor));
        const auto id = insert.inserted_id();
        impl_->audit("ban.added", actor, std::to_string(id), kind);
        return id;
    });
}

bool Store::remove_ban(std::uint64_t id, std::string_view actor) {
    const auto object = id == 0 ? std::string{} : std::to_string(id);
    return impl_->administrative("bans.remove", actor, object, [&] {
        if (id == 0) {
            throw std::invalid_argument("Invalid ban ID");
        }
        Statement remove(impl_->connection, "DELETE FROM bans WHERE id = ?", "remove ban");
        remove.execute(parameter(id));
        if (remove.affected_rows() == 0) {
            impl_->audit("admin.failed", actor, object, "bans.remove not_found");
            return false;
        }
        impl_->audit("ban.removed", actor, object, "ban deleted");
        return true;
    });
}

std::vector<Ban> Store::bans(std::uint64_t after_id, std::uint64_t limit) {
    validate_page_size(limit);
    UnsignedResult id;
    TextResult<3> kind;
    TextResult<45> identifier;
    TextResult<512 * 4> reason;
    TextResult<26> created, expires;
    TextResult<64> actor;
    Statement query(impl_->connection,
                    "SELECT id, kind, identifier, reason, created_at, expires_at, actor "
                    "FROM bans WHERE id > ? ORDER BY id LIMIT ?",
                    "read bans");
    query.execute(parameter(after_id), parameter(limit));
    query.results(id.column(), kind.column(), identifier.column(), reason.column(),
                  created.column(), expires.column(), actor.column());
    std::vector<Ban> result;
    while (query.next()) {
        const auto type = kind.text();
        if (type != "cid" && type != "ip") {
            throw Error("Unknown MariaDB ban kind");
        }
        result.push_back({id.value, type == "cid" ? BanKind::cid : BanKind::ip, identifier.text(),
                          reason.text(), created.text(), expires.nullable(), actor.nullable()});
    }
    return result;
}

std::optional<ActiveBan> Store::active_ban(std::string_view cid, std::string_view peer_ip) {
    const auto verified_cid = canonical_cid(cid);
    const auto ip = canonical_ip(peer_ip);
    UnsignedResult remaining;
    Statement query(impl_->connection, R"sql(
        SELECT CEILING(TIMESTAMPDIFF(MICROSECOND, UTC_TIMESTAMP(6), expires_at) / 1000000.0)
        FROM bans
        WHERE ((kind = 'cid' AND identifier = ?) OR (kind = 'ip' AND identifier = ?))
          AND (expires_at IS NULL OR expires_at > UTC_TIMESTAMP(6))
        ORDER BY expires_at IS NULL DESC, expires_at DESC
        LIMIT 1
    )sql",
                    "check active bans");
    query.execute(parameter(verified_cid), parameter(ip));
    query.results(remaining.column());
    if (!query.next()) {
        return std::nullopt;
    }
    return ActiveBan{remaining.is_null ? std::nullopt : std::optional{remaining.value}};
}

std::vector<AuditEvent> Store::audit_events(std::uint64_t after_id, std::uint64_t limit) {
    validate_page_size(limit);
    UnsignedResult id;
    TextResult<32> type;
    TextResult<26> created;
    TextResult<64> actor;
    TextResult<48> object;
    TextResult<96> details;
    Statement query(impl_->connection,
                    "SELECT id, event_type, created_at, actor, object_id, details "
                    "FROM audit_events WHERE id > ? ORDER BY id LIMIT ?",
                    "read audit events");
    query.execute(parameter(after_id), parameter(limit));
    query.results(id.column(), type.column(), created.column(), actor.column(), object.column(),
                  details.column());
    std::vector<AuditEvent> result;
    while (query.next()) {
        result.push_back({id.value, type.text(), created.text(), actor.nullable(),
                          object.nullable(), details.text()});
    }
    return result;
}

void Store::create_admin_user(std::string_view username, AdminRole role, std::string_view hash) {
    impl_->administrative("admin.create", {}, {}, [&] {
        if (!valid_admin_username(username) || !hash.starts_with("$argon2id$") ||
            hash.size() > 160 || hash.find('\0') != hash.npos) {
            throw std::invalid_argument("Invalid administrator credentials");
        }
        Statement insert(impl_->connection,
                         "INSERT INTO admin_users (username, password_hash, role) VALUES (?, ?, ?)",
                         "create administrator");
        insert.execute(parameter(username), parameter(hash), parameter(role_name(role)));
        impl_->audit("admin.created", {}, username, role_name(role));
        return true;
    });
}

std::optional<AdminCredentials> Store::admin_credentials(std::string_view username) {
    UnsignedResult id;
    TextResult<6> role;
    TextResult<160> hash;
    Statement query(impl_->connection,
                    "SELECT id, role, password_hash FROM admin_users WHERE username = ?",
                    "read administrator credentials");
    query.execute(parameter(username));
    query.results(id.column(), role.column(), hash.column());
    if (!query.next()) {
        return std::nullopt;
    }
    return AdminCredentials{{id.value, std::string(username), parse_role(role.text())},
                            hash.text()};
}

void Store::create_admin_session(const AdminAccount &account, std::string_view digest,
                                 std::uint64_t lifetime_seconds) {
    validate_digest(digest);
    if (lifetime_seconds == 0 || lifetime_seconds > 86400) {
        throw std::invalid_argument("Invalid administrator session lifetime");
    }
    auto id = account.id;
    {
        Statement prune(impl_->connection,
                        "DELETE FROM admin_sessions WHERE expires_at <= UTC_TIMESTAMP(6)",
                        "prune expired administrator sessions");
        prune.execute();
    }
    Transaction transaction(impl_->connection);
    Statement insert(impl_->connection,
                     "INSERT INTO admin_sessions (token_hash, admin_user_id, expires_at) "
                     "VALUES (UNHEX(?), ?, TIMESTAMPADD(SECOND, ?, UTC_TIMESTAMP(6)))",
                     "create administrator session");
    insert.execute(parameter(digest), parameter(id), parameter(lifetime_seconds));
    impl_->audit("auth.login", account.username, account.username, "session issued");
    transaction.commit();
}

std::optional<AdminAccount> Store::admin_session(std::string_view digest) {
    validate_digest(digest);
    UnsignedResult id;
    TextResult<48> name;
    TextResult<6> role;
    Statement query(impl_->connection,
                    "SELECT u.id, u.username, u.role FROM admin_sessions AS s "
                    "JOIN admin_users AS u ON u.id = s.admin_user_id "
                    "WHERE s.token_hash = UNHEX(?) AND s.expires_at > UTC_TIMESTAMP(6)",
                    "read administrator session");
    query.execute(parameter(digest));
    query.results(id.column(), name.column(), role.column());
    if (!query.next()) {
        return std::nullopt;
    }
    return AdminAccount{id.value, name.text(), parse_role(role.text())};
}

void Store::revoke_admin_session(const AdminAccount &account, std::string_view digest) {
    validate_digest(digest);
    auto id = account.id;
    Transaction transaction(impl_->connection);
    Statement remove(impl_->connection,
                     "DELETE FROM admin_sessions WHERE token_hash = UNHEX(?) AND admin_user_id = ?",
                     "delete administrator session");
    remove.execute(parameter(digest), parameter(id));
    if (remove.affected_rows() != 0) {
        impl_->audit("auth.logout", account.username, account.username, "session revoked");
    }
    transaction.commit();
}

std::uint64_t Store::revoke_admin_sessions(std::string_view username) {
    if (!valid_admin_username(username)) {
        throw std::invalid_argument("Invalid administrator username");
    }
    return impl_->administrative("admin.revoke", {}, username, [&] {
        Statement remove(impl_->connection,
                         "DELETE FROM admin_sessions WHERE admin_user_id = "
                         "(SELECT id FROM admin_users WHERE username = ?)",
                         "revoke administrator sessions");
        remove.execute(parameter(username));
        impl_->audit("admin.sessions.revoked", {}, username, "sessions revoked");
        return remove.affected_rows();
    });
}

} // namespace dc24h::db
