#include "dc24h/database/database.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <charconv>
#include <cerrno>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dc24h::database {
namespace {
constexpr std::uint64_t max_backup_bytes = 32u * 1024u * 1024u;
constexpr std::uint64_t max_backup_rows = 4096;
constexpr std::size_t max_cell_bytes = 65536;
constexpr std::string_view magic = "DC24HBK1";

struct Column { std::string_view name; bool binary; };
struct Table {
    std::string_view name;
    std::string_view order_by;
    const Column* columns;
    std::size_t count;
    bool application;
};
constexpr Column schema_columns[] = {
    {"version", false}, {"name", false}, {"checksum", false}, {"applied_at", false}, {"state", false},
};
constexpr Column account_columns[] = {
    {"id", false}, {"name", true}, {"role", true}, {"status", true},
    {"created_at", false}, {"updated_at", false}, {"last_login_at", false}, {"revision", false},
};
constexpr Column credential_columns[] = {{"account_id", false}, {"verifier", true}, {"created_at", false}};
constexpr Column identity_columns[] = {
    {"account_id", false}, {"nickname", true}, {"cid", true}, {"created_at", false},
};
constexpr Column adc_columns[] = {{"account_id", false}, {"secret", true}, {"created_at", false}};
constexpr Column ban_columns[] = {
    {"id", false}, {"kind", true}, {"subject", true}, {"reason", true}, {"created_at", false},
    {"expires_at", false}, {"created_by", false}, {"revoked_at", false}, {"revoked_by", false}, {"revision", false},
};
constexpr Column audit_columns[] = {
    {"id", false}, {"occurred_at", false}, {"action", true}, {"actor_id", false},
    {"target_kind", true}, {"target_id", false}, {"outcome", true},
};
constexpr Table tables[] = {
    {"dc24h_schema_migrations", "version", schema_columns, 5, false},
    {"dc24h_accounts", "id", account_columns, 8, true},
    {"dc24h_account_credentials", "account_id", credential_columns, 3, true},
    {"dc24h_account_identities", "account_id", identity_columns, 4, true},
    {"dc24h_account_adc_credentials", "account_id", adc_columns, 3, true},
    {"dc24h_bans", "id", ban_columns, 10, true},
    {"dc24h_audit_events", "id", audit_columns, 7, true},
};

[[noreturn]] void refuse() { throw std::runtime_error("Database backup refused"); }

std::string lock_name(Connection& connection) {
    const auto database = connection.scalar("SELECT DATABASE()");
    if (!database) { throw MigrationError(MigrationErrorKind::schema); }
    std::array<unsigned char, 32> digest{};
    unsigned int length = 0;
    if (EVP_Digest(database->data(), database->size(), digest.data(), &length, EVP_sha256(), nullptr) != 1 || length != digest.size()) {
        throw MigrationError(MigrationErrorKind::schema);
    }
    constexpr char hex[] = "0123456789abcdef";
    std::string name = "dc24h:";
    name.reserve(6 + 52);
    for (std::size_t i = 0; i < 26; ++i) {
        name.push_back(hex[digest[i] >> 4]);
        name.push_back(hex[digest[i] & 15]);
    }
    return name;
}

struct SessionLock {
    Connection& connection;
    std::string name;
    bool held{false};
    explicit SessionLock(Connection& open) : connection(open), name(lock_name(open)) {
        if (connection.scalar("SELECT IS_USED_LOCK(?) = CONNECTION_ID()", {name}) == "1" ||
            connection.scalar("SELECT GET_LOCK(?, 0)", {name}) != "1") {
            throw MigrationError(MigrationErrorKind::lock_unavailable);
        }
        held = true;
    }
    ~SessionLock() {
        if (!held) { return; }
        try { (void)connection.scalar("SELECT RELEASE_LOCK(?)", {name}); } catch (...) {}
    }
    SessionLock(const SessionLock&) = delete;
    SessionLock& operator=(const SessionLock&) = delete;
};

void require_current(Connection& connection) {
    if (migration_catalog().size() != 11) { throw MigrationError(MigrationErrorKind::history); }
    const auto status = inspect_migrations(connection);
    if (status.dirty) { throw MigrationError(MigrationErrorKind::dirty); }
    if (status.current_version != 11 || status.latest_version != 11 || status.pending != 0) {
        throw MigrationError(MigrationErrorKind::history);
    }
}

struct Split { std::string parent; std::string leaf; };
Split split_path(const std::string& path) {
    if (path.empty() || path.size() > 4096 || path.find('\0') != std::string::npos || path.back() == '/') { refuse(); }
    Split result;
    const auto slash = path.rfind('/');
    if (slash == std::string::npos) { result.parent = "."; result.leaf = path; }
    else if (slash == 0) { result.parent = "/"; result.leaf = path.substr(1); }
    else { result.parent = path.substr(0, slash); result.leaf = path.substr(slash + 1); }
    if (result.leaf.empty() || result.leaf == "." || result.leaf == "..") { refuse(); }
    return result;
}

struct Destination {
    int dir{-1};
    int file{-1};
    std::string leaf;
    bool created{false};
    bool commit{false};
    bool hashing{true};
    EVP_MD_CTX* digest{nullptr};
    std::uint64_t written{0};
    explicit Destination(const std::string& path) {
        const auto parts = split_path(path);
        leaf = parts.leaf;
        dir = ::open(parts.parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dir < 0) { refuse(); }
        file = ::openat(dir, leaf.c_str(), O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC | O_WRONLY, 0600);
        if (file < 0) { abandon(); refuse(); }
        created = true;
        struct stat state{};
        digest = EVP_MD_CTX_new();
        if (::fchmod(file, 0600) != 0 || ::fstat(file, &state) != 0 || !S_ISREG(state.st_mode) ||
            state.st_nlink != 1 || state.st_uid != ::geteuid() || (state.st_mode & 0777) != 0600 ||
            digest == nullptr || EVP_DigestInit_ex(digest, EVP_sha256(), nullptr) != 1) {
            abandon(); refuse();
        }
    }
    ~Destination() {
        if (!commit) { abandon(); }
        else {
            if (digest != nullptr) { EVP_MD_CTX_free(digest); }
            if (file >= 0) { ::close(file); }
            if (dir >= 0) { ::close(dir); }
        }
    }
    Destination(const Destination&) = delete;
    Destination& operator=(const Destination&) = delete;
    void abandon() noexcept {
        if (created && dir >= 0) { ::unlinkat(dir, leaf.c_str(), 0); }
        created = false;
        if (digest != nullptr) { EVP_MD_CTX_free(digest); digest = nullptr; }
        if (file >= 0) { ::close(file); file = -1; }
        if (dir >= 0) { ::close(dir); dir = -1; }
    }
    void emit(const void* data, std::size_t size) {
        const auto ceiling = hashing ? max_backup_bytes - 32 : max_backup_bytes;
        if (size > ceiling || written > ceiling - size) { throw Error(ErrorKind::limit, Operation::execute); }
        const auto* bytes = static_cast<const unsigned char*>(data);
        std::size_t done = 0;
        while (done < size) {
            const auto count = ::write(file, bytes + done, size - done);
            if (count < 0 && errno == EINTR) { continue; }
            if (count <= 0) { refuse(); }
            done += static_cast<std::size_t>(count);
        }
        if (hashing && EVP_DigestUpdate(digest, bytes, size) != 1) { refuse(); }
        written += size;
    }
    void emit_u8(unsigned char value) { emit(&value, 1); }
    void emit_u16(std::uint16_t value) {
        const unsigned char bytes[] = {
            static_cast<unsigned char>(value & 0xff), static_cast<unsigned char>((value >> 8) & 0xff)};
        emit(bytes, 2);
    }
    void emit_u32(std::uint32_t value) {
        const unsigned char bytes[] = {
            static_cast<unsigned char>(value & 0xff), static_cast<unsigned char>((value >> 8) & 0xff),
            static_cast<unsigned char>((value >> 16) & 0xff), static_cast<unsigned char>((value >> 24) & 0xff)};
        emit(bytes, 4);
    }
    void emit_text(std::string_view value) {
        if (value.size() > 65535) { refuse(); }
        emit_u16(static_cast<std::uint16_t>(value.size()));
        emit(value.data(), value.size());
    }
    void finish() {
        std::array<unsigned char, 32> sum{};
        unsigned int length = 0;
        if (EVP_DigestFinal_ex(digest, sum.data(), &length) != 1 || length != sum.size()) { refuse(); }
        hashing = false;
        emit(sum.data(), sum.size());
        for (;;) {
            if (::fsync(file) == 0 && ::fsync(dir) == 0) { return; }
            if (errno != EINTR) { refuse(); }
        }
    }
};

std::uint64_t count_rows(Connection& connection, const Table& table) {
    const auto value = connection.scalar(std::string("SELECT COUNT(*) FROM ") + std::string(table.name));
    if (!value) { throw Error(ErrorKind::result_shape, Operation::fetch); }
    std::uint64_t count{};
    const auto* end = value->data() + value->size();
    const auto [stop, error] = std::from_chars(value->data(), end, count);
    if (error != std::errc{} || stop != end || count > max_backup_rows) { throw Error(ErrorKind::limit, Operation::fetch); }
    return count;
}

std::string select_sql(const Table& table) {
    std::string sql = "SELECT ";
    for (std::size_t i = 0; i < table.count; ++i) {
        if (i != 0) { sql += ','; }
        sql += table.columns[i].name;
    }
    sql += " FROM ";
    sql += table.name;
    sql += " ORDER BY ";
    sql += table.order_by;
    sql += " LIMIT 256 OFFSET ?";
    return sql;
}

std::vector<Row> read_rows(Connection& connection, const Table& table, std::uint64_t expected) {
    const auto sql = select_sql(table);
    std::vector<Row> rows;
    rows.reserve(static_cast<std::size_t>(expected));
    const QueryLimits limits{256, table.count, max_cell_bytes, 4u * 1024u * 1024u};
    for (std::uint64_t offset = 0; offset < expected; offset += 256) {
        const auto page = connection.query(sql, Parameters{static_cast<std::int64_t>(offset)}, limits);
        if (page.columns.size() != table.count || page.rows.size() > 256) { throw Error(ErrorKind::result_shape, Operation::fetch); }
        for (std::size_t column = 0; column < table.count; ++column) {
            if (page.columns[column] != table.columns[column].name) { throw Error(ErrorKind::result_shape, Operation::fetch); }
        }
        for (const auto& row : page.rows) {
            if (row.size() != table.count) { throw Error(ErrorKind::result_shape, Operation::fetch); }
            rows.push_back(row);
        }
        if (page.rows.size() < 256) { break; }
    }
    if (rows.size() != expected) { throw Error(ErrorKind::result_shape, Operation::fetch); }
    return rows;
}

std::uint64_t write_tables(Destination& destination, Connection& connection) {
    destination.emit(magic.data(), magic.size());
    destination.emit_u32(10);
    destination.emit_u32(static_cast<std::uint32_t>(std::size(tables)));
    std::uint64_t total = 0;
    for (const auto& table : tables) {
        const auto count = count_rows(connection, table);
        const auto rows = read_rows(connection, table, count);
        destination.emit_text(table.name);
        destination.emit_u16(static_cast<std::uint16_t>(table.count));
        for (std::size_t column = 0; column < table.count; ++column) { destination.emit_text(table.columns[column].name); }
        destination.emit_u32(static_cast<std::uint32_t>(count));
        for (const auto& row : rows) {
            for (const auto& cell : row) {
                if (!cell) { destination.emit_u8(0); continue; }
                if (cell->size() > max_cell_bytes) { throw Error(ErrorKind::limit, Operation::fetch); }
                destination.emit_u8(1);
                destination.emit_u32(static_cast<std::uint32_t>(cell->size()));
                destination.emit(cell->data(), cell->size());
            }
        }
        total += count;
    }
    return total;
}

struct Cursor {
    const unsigned char* cursor;
    const unsigned char* end;
    explicit Cursor(std::string_view body)
        : cursor(reinterpret_cast<const unsigned char*>(body.data())), end(cursor + body.size()) {}
    void need(std::size_t size) const { if (static_cast<std::size_t>(end - cursor) < size) { refuse(); } }
    unsigned char u8() { need(1); return *cursor++; }
    std::uint16_t u16() {
        need(2);
        const auto value = static_cast<std::uint16_t>(cursor[0] | (static_cast<unsigned int>(cursor[1]) << 8));
        cursor += 2; return value;
    }
    std::uint32_t u32() {
        need(4);
        const auto value = static_cast<std::uint32_t>(cursor[0] | (static_cast<unsigned int>(cursor[1]) << 8) |
            (static_cast<unsigned int>(cursor[2]) << 16) | (static_cast<unsigned int>(cursor[3]) << 24));
        cursor += 4; return value;
    }
    std::string_view bytes(std::size_t size) {
        need(size);
        const std::string_view value(reinterpret_cast<const char*>(cursor), size);
        cursor += size; return value;
    }
    std::string text() { return std::string(bytes(u16())); }
    [[nodiscard]] bool done() const { return cursor == end; }
};

std::vector<std::vector<Row>> parse_body(std::string_view body, std::uint32_t& version) {
    Cursor cursor(body);
    if (cursor.bytes(magic.size()) != magic) { refuse(); }
    version = cursor.u32();
    // The v9 archive uses the same table/column encoding. Its migration
    // history is a verified prefix of v10, so it remains restorable.
    if ((version != 9 && version != 10) || cursor.u32() != std::size(tables)) { refuse(); }
    std::vector<std::vector<Row>> stored;
    stored.reserve(std::size(tables));
    for (const auto& table : tables) {
        if (cursor.text() != table.name || cursor.u16() != table.count) { refuse(); }
        for (std::size_t column = 0; column < table.count; ++column) {
            if (cursor.text() != table.columns[column].name) { refuse(); }
        }
        const auto count = cursor.u32();
        if (count > max_backup_rows) { refuse(); }
        std::vector<Row> rows;
        rows.reserve(count);
        for (std::uint32_t row_index = 0; row_index < count; ++row_index) {
            Row row;
            row.reserve(table.count);
            for (std::size_t column = 0; column < table.count; ++column) {
                const auto tag = cursor.u8();
                if (tag == 0) { row.emplace_back(std::nullopt); continue; }
                if (tag != 1) { refuse(); }
                const auto size = cursor.u32();
                if (size > max_cell_bytes) { refuse(); }
                row.emplace_back(std::string(cursor.bytes(size)));
            }
            rows.push_back(std::move(row));
        }
        stored.push_back(std::move(rows));
    }
    if (!cursor.done()) { refuse(); }
    return stored;
}

std::string read_private_backup(const std::string& path) {
    const auto parts = split_path(path);
    const int dir = ::open(parts.parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0) { refuse(); }
    const int file = ::openat(dir, parts.leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    ::close(dir);
    if (file < 0) { refuse(); }
    struct Close { int fd; ~Close() { ::close(fd); } } close{file};
    struct stat state{};
    if (::fstat(file, &state) != 0 || !S_ISREG(state.st_mode) || state.st_nlink != 1 || state.st_uid != ::geteuid() ||
        (state.st_mode & 0077) != 0 || (state.st_mode & 0400) == 0 || state.st_size < 48 ||
        static_cast<std::uint64_t>(state.st_size) > max_backup_bytes) {
        refuse();
    }
    std::string body(static_cast<std::size_t>(state.st_size), '\0');
    std::size_t done = 0;
    while (done < body.size()) {
        const auto count = ::read(file, body.data() + done, body.size() - done);
        if (count < 0 && errno == EINTR) { continue; }
        if (count <= 0) { refuse(); }
        done += static_cast<std::size_t>(count);
    }
    constexpr std::size_t digest_bytes = 32;
    if (body.size() < digest_bytes) { refuse(); }
    const auto digest_at = body.size() - digest_bytes;
    std::array<unsigned char, digest_bytes> digest{};
    unsigned int length = 0;
    if (EVP_Digest(body.data(), digest_at, digest.data(), &length, EVP_sha256(), nullptr) != 1 || length != digest.size() ||
        CRYPTO_memcmp(digest.data(), body.data() + digest_at, digest.size()) != 0) {
        refuse();
    }
    body.resize(digest_at);
    return body;
}

std::string insert_sql(const Table& table) {
    std::string sql = "INSERT INTO ";
    sql += table.name;
    sql += " (";
    for (std::size_t i = 0; i < table.count; ++i) {
        if (i != 0) { sql += ','; }
        sql += table.columns[i].name;
    }
    sql += ") VALUES (";
    for (std::size_t i = 0; i < table.count; ++i) {
        if (i != 0) { sql += ','; }
        sql += '?';
    }
    sql += ')';
    return sql;
}

void same_rows(const std::vector<Row>& live, const std::vector<Row>& stored) {
    if (live.size() != stored.size()) { refuse(); }
    for (std::size_t row = 0; row < live.size(); ++row) {
        if (live[row].size() != stored[row].size()) { refuse(); }
        for (std::size_t column = 0; column < live[row].size(); ++column) {
            if (live[row][column] != stored[row][column]) { refuse(); }
        }
    }
}

std::uint64_t insert_rows(Connection& connection, const std::vector<std::vector<Row>>& stored,
    std::uint32_t archive_version) {
    std::uint64_t inserted = 0;
    for (std::size_t index = 0; index < std::size(tables); ++index) {
        const auto& table = tables[index];
        if (!table.application) {
            const auto live = read_rows(connection, table, count_rows(connection, table));
            if (archive_version == 9 && index == 0) {
                if (live.size() != stored[index].size() + 1 || stored[index].size() != 9) { refuse(); }
                same_rows(std::vector<Row>(live.begin(),live.begin()+9),stored[index]);
            } else { same_rows(live,stored[index]); }
            continue;
        }
        if (count_rows(connection, table) != 0) { refuse(); }
    }
    for (std::size_t index = 0; index < std::size(tables); ++index) {
        const auto& table = tables[index];
        if (!table.application) { continue; }
        const auto sql = insert_sql(table);
        for (const auto& row : stored[index]) {
            Parameters parameters;
            parameters.reserve(table.count);
            for (std::size_t column = 0; column < table.count; ++column) {
                const auto& cell = row[column];
                if (!cell) { parameters.emplace_back(nullptr); }
                else if (table.columns[column].binary) { parameters.emplace_back(Bytes(cell->begin(), cell->end())); }
                else { parameters.emplace_back(*cell); }
            }
            if (connection.execute_prepared(sql, parameters).affected_rows != 1) { throw Error(ErrorKind::statement, Operation::execute); }
            ++inserted;
        }
    }
    return inserted;
}
} // namespace

BackupReport backup(Connection& connection, const std::string& path) {
    if (connection.in_transaction()) { throw Error(ErrorKind::transaction, Operation::begin); }
    const SessionLock lock(connection);
    require_current(connection);
    Destination destination(path);
    auto transaction = connection.transaction(Snapshot::consistent);
    const auto rows = write_tables(destination, connection);
    destination.finish();
    transaction.rollback();
    const auto bytes = destination.written;
    destination.commit = true;
    return BackupReport{bytes, rows};
}

BackupReport restore(Connection& connection, const std::string& path) {
    if (connection.in_transaction()) { throw Error(ErrorKind::transaction, Operation::begin); }
    const auto body = read_private_backup(path);
    std::uint32_t archive_version{};
    const auto stored = parse_body(body,archive_version);
    const SessionLock lock(connection);
    require_current(connection);
    auto transaction = connection.transaction();
    const auto rows = insert_rows(connection, stored,archive_version);
    transaction.commit();
    return BackupReport{body.size() + 32, rows};
}
} // namespace dc24h::database
