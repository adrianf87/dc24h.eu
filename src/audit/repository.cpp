#include "dc24h/audit/repository.hpp"
#include <array>
#include <charconv>
#include <limits>

namespace dc24h::audit {
namespace {
[[noreturn]] void fail(ErrorKind kind) { throw Error(kind); }
constexpr auto maximum = std::uint64_t(std::numeric_limits<std::int64_t>::max());
constexpr std::array<std::string_view,15> actions{
    "accounts.create","accounts.activate","accounts.disable","accounts.password","accounts.reset","accounts.role",
    "ownership.bootstrap","ownership.transfer","bans.create","bans.revoke","configuration.write","configuration.restore",
    "hub.start","hub.stop","hub.restart"};
bool identifier(std::uint64_t value) noexcept { return value && value <= maximum; }
std::uint64_t number(const database::Cell& cell) {
    if (!cell || cell->empty() || cell->size() > 20) { fail(ErrorKind::invalid_record); }
    std::uint64_t value{};
    const auto [end,error] = std::from_chars(cell->data(), cell->data()+cell->size(), value);
    if (error != std::errc{} || end != cell->data()+cell->size() || std::to_string(value) != *cell || !identifier(value)) {
        fail(ErrorKind::invalid_record);
    }
    return value;
}
std::int64_t timestamp(const database::Cell& cell) {
    if (!cell || cell->empty() || cell->size() > 12) { fail(ErrorKind::invalid_record); }
    std::int64_t value{};
    const auto [end,error] = std::from_chars(cell->data(), cell->data()+cell->size(), value);
    if (error != std::errc{} || end != cell->data()+cell->size() || std::to_string(value) != *cell || value < 0 || value > 253402300799) {
        fail(ErrorKind::invalid_record);
    }
    return value;
}
}
bool known_action(std::string_view action) noexcept {
    for (const auto key : actions) { if (key == action) { return true; } }
    return false;
}
const char* Error::what() const noexcept {
    switch (kind_) {
    case ErrorKind::invalid_input: return "Invalid audit request";
    case ErrorKind::schema: return "Audit history requires the current schema";
    case ErrorKind::invalid_record: return "Invalid stored audit event";
    }
    return "Invalid audit request";
}
void Repository::require_schema() {
    const auto status = database::inspect_migrations(connection_);
    if (status.dirty || status.pending || status.current_version < 9) { fail(ErrorKind::schema); }
    if (connection_.scalar("SELECT ENGINE FROM information_schema.tables WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_audit_events'") != "InnoDB") {
        fail(ErrorKind::schema);
    }
    const auto columns = connection_.query(
        "SELECT COLUMN_NAME,DATA_TYPE,IS_NULLABLE,CHARACTER_MAXIMUM_LENGTH,EXTRA,COLUMN_TYPE FROM information_schema.columns "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_audit_events' ORDER BY ORDINAL_POSITION", {}, {7,6,128,4096});
    constexpr std::array<std::string_view,7> names{"id","occurred_at","action","actor_id","target_kind","target_id","outcome"};
    if (columns.rows.size() != names.size()) { fail(ErrorKind::schema); }
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto& row = columns.rows[i];
        const bool bytes = i == 2 || i == 4 || i == 6;
        if (row[0] != names[i] || row[1] != (bytes ? "varbinary" : "bigint") || row[2] != "NO" || row[4] != (i == 0 ? "auto_increment" : "") ||
            (bytes && row[3] != (i == 2 ? "32" : "16")) || (!bytes && (!row[5] || row[5]->find("unsigned") != std::string::npos))) {
            fail(ErrorKind::schema);
        }
    }
    if (connection_.scalar("SELECT COUNT(*) FROM information_schema.key_column_usage WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_audit_events' AND CONSTRAINT_NAME='dc24h_audit_events_actor' AND COLUMN_NAME='actor_id' AND REFERENCED_TABLE_SCHEMA=DATABASE() AND REFERENCED_TABLE_NAME='dc24h_accounts' AND REFERENCED_COLUMN_NAME='id'") != "1" ||
        connection_.scalar("SELECT COUNT(*) FROM information_schema.statistics WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_audit_events' AND INDEX_NAME='PRIMARY' AND COLUMN_NAME='id' AND NON_UNIQUE=0 AND SEQ_IN_INDEX=1 AND SUB_PART IS NULL") != "1") {
        fail(ErrorKind::schema);
    }
}
void Repository::append_in_transaction(std::string_view action, std::uint64_t actor, std::string_view target_kind,
    std::uint64_t target, std::int64_t occurred_at) {
    if (!connection_.in_transaction()) { throw database::Error(database::ErrorKind::transaction, database::Operation::begin); }
    if (!known_action(action) || action=="hub.start" || action=="hub.stop" || action=="hub.restart" ||
        (target_kind != "account" && target_kind != "ban") || !identifier(actor) || !identifier(target) ||
        occurred_at < 0 || occurred_at > 253402300799) { fail(ErrorKind::invalid_input); }
    database::Parameters values;
    values.reserve(5);
    values.emplace_back(occurred_at);
    values.emplace_back(std::string(action));
    values.emplace_back(actor);
    values.emplace_back(std::string(target_kind));
    values.emplace_back(target);
    const auto written = connection_.execute_prepared(
        "INSERT INTO dc24h_audit_events(occurred_at,action,actor_id,target_kind,target_id,outcome) VALUES (?,?,?,?,?,'succeeded')",
        values);
    if (written.affected_rows != 1 || !written.insert_id) { fail(ErrorKind::invalid_record); }
}
std::uint64_t Repository::begin_configuration(std::string_view action, std::uint64_t actor, std::int64_t occurred_at) {
    if (connection_.in_transaction() || (action != "configuration.write" && action != "configuration.restore") ||
        !identifier(actor) || occurred_at < 0 || occurred_at > 253402300799) { fail(ErrorKind::invalid_input); }
    require_schema();
    const auto status = database::inspect_migrations(connection_);
    if (status.current_version < 10 || status.pending || status.dirty) { fail(ErrorKind::schema); }
    const auto result = connection_.execute_prepared(
        "INSERT INTO dc24h_audit_events(occurred_at,action,actor_id,target_kind,target_id,outcome) "
        "VALUES (?,?,?,'configuration',1,'started')",
        {occurred_at,std::string(action),actor});
    if (result.affected_rows != 1 || !identifier(result.insert_id)) { fail(ErrorKind::invalid_record); }
    return result.insert_id;
}
void Repository::finish_configuration(std::uint64_t event_id, std::uint64_t actor, bool succeeded) {
    if (connection_.in_transaction() || !identifier(event_id) || !identifier(actor)) { fail(ErrorKind::invalid_input); }
    const auto result = connection_.execute_prepared(
        "UPDATE dc24h_audit_events SET outcome=? WHERE id=? AND actor_id=? "
        "AND target_kind='configuration' AND outcome='started'",
        {std::string(succeeded ? "succeeded" : "failed"),event_id,actor});
    if (result.affected_rows != 1) { fail(ErrorKind::invalid_record); }
}
std::uint64_t Repository::begin_service(std::string_view action, std::uint64_t actor, std::int64_t occurred_at) {
    if (connection_.in_transaction() || (action!="hub.start"&&action!="hub.stop"&&action!="hub.restart") ||
        !identifier(actor) || occurred_at < 0 || occurred_at > 253402300799) { fail(ErrorKind::invalid_input); }
    require_schema();
    const auto status=database::inspect_migrations(connection_);
    if(status.current_version<11||status.pending||status.dirty){fail(ErrorKind::schema);}
    const auto result=connection_.execute_prepared(
        "INSERT INTO dc24h_audit_events(occurred_at,action,actor_id,target_kind,target_id,outcome) "
        "VALUES (?,?,?,'service',1,'started')",{occurred_at,std::string(action),actor});
    if(result.affected_rows!=1||!identifier(result.insert_id)){fail(ErrorKind::invalid_record);}
    return result.insert_id;
}
void Repository::finish_service(std::uint64_t event_id, std::uint64_t actor, bool succeeded) {
    if(connection_.in_transaction()||!identifier(event_id)||!identifier(actor)){fail(ErrorKind::invalid_input);}
    const auto result=connection_.execute_prepared(
        "UPDATE dc24h_audit_events SET outcome=? WHERE id=? AND actor_id=? "
        "AND target_kind='service' AND outcome='started'",
        {std::string(succeeded?"succeeded":"failed"),event_id,actor});
    if(result.affected_rows!=1){fail(ErrorKind::invalid_record);}
}
std::vector<Event> Repository::list(std::uint64_t after) {
    if (after > maximum) { fail(ErrorKind::invalid_input); }
    require_schema();
    database::Parameters cursor;
    cursor.emplace_back(after);
    const auto rows = connection_.query(
        "SELECT id,occurred_at,action,actor_id,target_kind,target_id,outcome FROM dc24h_audit_events WHERE id>? ORDER BY id LIMIT 16",
        cursor, {page_size,7,64,8192});
    std::vector<Event> page;
    page.reserve(rows.rows.size());
    for (const auto& row : rows.rows) {
        if (row.size() != 7 || !row[2] || !known_action(*row[2]) || !row[4] || !row[6]) {
            fail(ErrorKind::invalid_record);
        }
        const bool completed = *row[6] == "succeeded";
        const bool transitional = *row[6] == "started" || completed || *row[6] == "failed";
        const bool legacy = (*row[4] == "account" || *row[4] == "ban") && completed &&
            *row[2] != "configuration.write" && *row[2] != "configuration.restore" &&
            *row[2] != "hub.start" && *row[2] != "hub.stop" && *row[2] != "hub.restart";
        const bool configuration = *row[4] == "configuration" && row[5] && *row[5] == "1" &&
            (*row[2] == "configuration.write" || *row[2] == "configuration.restore") && transitional;
        const bool service = *row[4] == "service" && row[5] && *row[5] == "1" &&
            (*row[2] == "hub.start" || *row[2] == "hub.stop" || *row[2] == "hub.restart") && transitional;
        if (!legacy && !configuration && !service) {
            fail(ErrorKind::invalid_record);
        }
        Event event{number(row[0]), timestamp(row[1]), *row[2], number(row[3]), *row[4], number(row[5]), *row[6]};
        if (!page.empty() && event.id <= page.back().id) { fail(ErrorKind::invalid_record); }
        page.push_back(std::move(event));
    }
    return page;
}
} // namespace dc24h::audit
