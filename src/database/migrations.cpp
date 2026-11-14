#include "migrations_internal.hpp"
#include "dc24h/database/migrations.hpp"

#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <string>

namespace dc24h::database {
namespace {
const char* diagnostic(MigrationErrorKind kind) noexcept {
    switch (kind) {
    case MigrationErrorKind::catalog: return "Invalid database migration catalog";
    case MigrationErrorKind::lock_unavailable: return "Database migration lock unavailable";
    case MigrationErrorKind::history: return "Database migration history mismatch";
    case MigrationErrorKind::dirty: return "Database migration requires operator recovery";
    case MigrationErrorKind::schema: return "Database migration metadata schema mismatch";
    }
    return "Database migration history mismatch";
}
[[noreturn]] void fail(MigrationErrorKind kind) { throw MigrationError(kind); }
std::string digest(std::string_view value) {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> bytes{};
    SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), bytes.data());
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (const auto byte : bytes) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
struct History {
    MigrationStatus status;
    bool exists{false}, state_column{false};
};
History read_history(Connection& c, std::span<const detail::Entry> entries) {
    History h;
    h.status.latest_version = static_cast<unsigned int>(entries.size());
    h.status.pending = h.status.latest_version;
    const auto table = c.query("SELECT ENGINE FROM information_schema.tables WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_schema_migrations'",
                               {}, QueryLimits{1,1,64,64});
    if (table.rows.empty()) { return h; }
    if (table.rows.front().front() != "InnoDB") { fail(MigrationErrorKind::schema); }
    h.exists = true;
    const auto columns = c.query("SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE,CHARACTER_SET_NAME,COLLATION_NAME,CHARACTER_MAXIMUM_LENGTH,COLUMN_DEFAULT "
        "FROM information_schema.columns WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_schema_migrations' ORDER BY ORDINAL_POSITION",
        {}, QueryLimits{6,8,256,4096});
    if (columns.rows.size() != 4 && columns.rows.size() != 5) { fail(MigrationErrorKind::schema); }
    constexpr std::array<std::string_view,5> names{"version","name","checksum","applied_at","state"};
    for (std::size_t i = 0; i < columns.rows.size(); ++i) {
        const auto& row = columns.rows[i];
        if (row[0] != names[i] || row[3] != "NO") { fail(MigrationErrorKind::schema); }
        if (i == 0) {
            if (row[1] != "int" || !row[2] || row[2]->find("unsigned") == std::string::npos) { fail(MigrationErrorKind::schema); }
        } else if (i == 3) {
            if (row[2] != "timestamp") { fail(MigrationErrorKind::schema); }
        } else {
            const auto length = i == 1 ? "128" : i == 2 ? "64" : "16";
            if (row[1] != (i == 2 ? "char" : "varchar") || row[4] != "ascii" || row[5] != "ascii_bin" || row[6] != length) {
                fail(MigrationErrorKind::schema);
            }
            if (i == 4 && row[7] != "'applied'" && row[7] != "applied") { fail(MigrationErrorKind::schema); }
        }
    }
    if (c.scalar("SELECT COUNT(*) FROM information_schema.statistics WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_schema_migrations' AND INDEX_NAME='PRIMARY'") != "1" ||
        c.scalar("SELECT COLUMN_NAME FROM information_schema.statistics WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_schema_migrations' AND INDEX_NAME='PRIMARY' AND SEQ_IN_INDEX=1") != "version") {
        fail(MigrationErrorKind::schema);
    }
    h.state_column = columns.rows.size() == 5;
    const auto history = c.query(h.state_column ?
        "SELECT version,name,checksum,state FROM dc24h_schema_migrations ORDER BY version" :
        "SELECT version,name,checksum,'applied' FROM dc24h_schema_migrations ORDER BY version",
        {}, QueryLimits{129,4,128,65536});
    if (history.rows.size() > entries.size() || (history.rows.empty() && h.state_column)) { fail(MigrationErrorKind::history); }
    for (std::size_t i = 0; i < history.rows.size(); ++i) {
        const auto& row = history.rows[i];
        const auto& info = entries[i].info;
        if (row[0] != std::to_string(info.version) || row[1] != info.name || row[2] != info.checksum) { fail(MigrationErrorKind::history); }
        if (row[3] == "applying" && i >= 2 && i + 1 == history.rows.size()) { h.status.dirty = true; }
        else if (row[3] == "applied" && !h.status.dirty) { ++h.status.current_version; }
        else { fail(MigrationErrorKind::history); }
    }
    if (!h.state_column && history.rows.size() > 1) { fail(MigrationErrorKind::schema); }
    h.status.pending -= h.status.current_version;
    return h;
}
std::string lock_name(Connection& c) {
    const auto name = c.scalar("SELECT DATABASE()");
    if (!name) { fail(MigrationErrorKind::schema); }
    return "dc24h:" + digest(*name).substr(0,52);
}
void verify_lock(Connection& c, const std::string& name) {
    if (c.scalar("SELECT IS_USED_LOCK(?) = CONNECTION_ID()", {name}) != "1") { fail(MigrationErrorKind::lock_unavailable); }
}
} // namespace

MigrationError::MigrationError(MigrationErrorKind kind) : std::runtime_error(diagnostic(kind)), kind_(kind) {}

namespace detail {
void validate_catalog(std::span<const Entry> entries) {
    if (entries.size() < 2 || entries.size() > 128) { fail(MigrationErrorKind::catalog); }
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const auto& info = entry.info;
        if (info.version != i + 1 || info.name.empty() || info.name.size() > 128 ||
            info.name.front() < 'a' || info.name.front() > 'z' ||
            !std::all_of(info.name.begin(),info.name.end(),[](char x) { return (x >= 'a' && x <= 'z') || (x >= '0' && x <= '9') || x == '_'; }) ||
            entry.statements.empty() || entry.statements.size() > 32) { fail(MigrationErrorKind::catalog); }
        std::string input;
        for (const auto sql : entry.statements) {
            if (sql.empty() || sql.size() > max_sql_bytes || sql.find('\0') != std::string_view::npos) { fail(MigrationErrorKind::catalog); }
            if (entry.statements.size() > 1) { input += std::to_string(sql.size()) + ':'; }
            input += sql;
        }
        if (digest(input) != info.checksum) { fail(MigrationErrorKind::catalog); }
    }
    // These are the only retryable bootstrap steps, never application SQL.
    if (entries[0].info.name != "schema_migrations" || entries[1].info.name != "migration_state" ||
        entries[0].statements.size() != 1 || entries[1].statements.size() != 1) { fail(MigrationErrorKind::catalog); }
}
MigrationStatus inspect(Connection& c, std::span<const Entry> entries) {
    if (c.in_transaction()) { throw Error(ErrorKind::transaction, Operation::begin); }
    validate_catalog(entries);
    return read_history(c,entries).status;
}
unsigned int run(Connection& c, std::span<const Entry> entries) {
    if (c.in_transaction()) { throw Error(ErrorKind::transaction, Operation::begin); }
    validate_catalog(entries);
    const auto lock = lock_name(c);
    // Refuse reentrant acquisition: GET_LOCK is reference-counted in MariaDB.
    if (c.scalar("SELECT IS_USED_LOCK(?) = CONNECTION_ID()", {lock}) == "1" ||
        c.scalar("SELECT GET_LOCK(?, 0)", {lock}) != "1") { fail(MigrationErrorKind::lock_unavailable); }
    struct Unlock {
        Connection& c; const std::string& name;
        ~Unlock() { try { (void)c.scalar("SELECT RELEASE_LOCK(?)", {name}); } catch (...) {} }
    } unlock{c,lock};
    auto history = read_history(c,entries);
    if (history.status.dirty) { fail(MigrationErrorKind::dirty); }
    unsigned int applied = 0;
    for (std::size_t i = history.status.current_version; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const auto version = std::to_string(entry.info.version);
        const std::vector<std::string> identity{version,std::string(entry.info.name),std::string(entry.info.checksum)};
        verify_lock(c,lock);
        if (i >= 2) {
            // Durable intent precedes DDL; errors or lost acknowledgements leave
            // a dirty row. Never retry arbitrary SQL after a partial migration.
            c.execute("INSERT INTO dc24h_schema_migrations(version,name,checksum,state) VALUES (?,?,?,'applying')", identity);
        }
        for (const auto sql : entry.statements) { verify_lock(c,lock); c.execute(sql); }
        verify_lock(c,lock);
        if (i < 2) {
            // Original v1 and the idempotent state-column v2 can safely resume
            // after DDL success but before recording their applied identity.
            c.execute("INSERT INTO dc24h_schema_migrations(version,name,checksum) VALUES (?,?,?)", identity);
        } else {
            const auto done = c.execute_prepared("UPDATE dc24h_schema_migrations SET state='applied',applied_at=CURRENT_TIMESTAMP WHERE version=? AND state='applying'",
                                                  {static_cast<std::uint64_t>(entry.info.version)});
            if (done.affected_rows != 1) { fail(MigrationErrorKind::history); }
        }
        // Validate metadata structure and the entire prefix after every step.
        history = read_history(c,entries);
        if (history.status.dirty || history.status.current_version != i + 1) { fail(MigrationErrorKind::history); }
        ++applied;
    }
    return applied;
}
} // namespace detail
std::vector<MigrationInfo> migration_catalog() {
    std::vector<MigrationInfo> result;
    for (const auto& entry : detail::builtin_migrations) { result.push_back(entry.info); }
    return result;
}
MigrationStatus inspect_migrations(Connection& c) { return detail::inspect(c,detail::builtin_migrations); }
unsigned int migrate(Connection& c) { return detail::run(c,detail::builtin_migrations); }
} // namespace dc24h::database
