#include "dc24h/accounts/repository.hpp"

#include <charconv>
#include <array>
#include <limits>
#include <type_traits>
#include <utility>

namespace dc24h::accounts {
namespace {
constexpr std::uint64_t max_revision = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
constexpr std::string_view by_id =
    "SELECT id,name,role,status,created_at,updated_at,last_login_at,revision FROM dc24h_accounts WHERE id=?";
constexpr std::string_view by_name =
    "SELECT id,name,role,status,created_at,updated_at,last_login_at,revision FROM dc24h_accounts WHERE name=?";
constexpr std::string_view locked =
    "SELECT id,name,role,status,created_at,updated_at,last_login_at,revision FROM dc24h_accounts WHERE id=? FOR UPDATE";
constexpr database::QueryLimits limits{1,8,64,512};
static_assert(std::is_nothrow_move_constructible_v<StoredAccount>);
template<class... T> database::Parameters parameters(T&&... values) {
    // Construct alternatives in their destination; do not copy string variants
    // through an initializer_list during allocation-failure unwinding.
    database::Parameters result;
    result.reserve(sizeof...(T));
    (result.emplace_back(std::forward<T>(values)),...);
    return result;
}
[[noreturn]] void fail(RepositoryErrorKind kind) { throw RepositoryError(kind); }
template<class T> T number(const database::Cell& cell) {
    if (!cell || cell->empty()) { fail(RepositoryErrorKind::invalid_record); }
    T value{};
    const auto [end,error] = std::from_chars(cell->data(),cell->data()+cell->size(),value);
    if (error != std::errc{} || end != cell->data()+cell->size() || std::to_string(value) != *cell) {
        fail(RepositoryErrorKind::invalid_record);
    }
    return value;
}
std::optional<StoredAccount> decode(const database::Result& result) {
    if (result.rows.empty()) { return std::nullopt; }
    if (result.rows.size() != 1 || result.rows[0].size() != 8) { fail(RepositoryErrorKind::invalid_record); }
    const auto& row = result.rows[0];
    if (!row[1] || !row[2] || !row[3]) { fail(RepositoryErrorKind::invalid_record); }
    const auto role = parse_role(*row[2]);
    const auto status = parse_status(*row[3]);
    if (!role || !status) { fail(RepositoryErrorKind::invalid_record); }
    Times times{number<Timestamp>(row[4]),number<Timestamp>(row[5]),std::nullopt};
    if (row[6]) { times.last_login_at = number<Timestamp>(row[6]); }
    auto value = Account::create({number<std::uint64_t>(row[0]),*row[1],*role,*status,times});
    const auto revision = number<std::uint64_t>(row[7]);
    if (!std::holds_alternative<Account>(value) || revision == 0 || revision > max_revision) {
        fail(RepositoryErrorKind::invalid_record);
    }
    return StoredAccount{std::get<Account>(std::move(value)),revision};
}
database::Parameter login(const Times& times) {
    return times.last_login_at ? database::Parameter{*times.last_login_at} : database::Parameter{nullptr};
}
void duplicate(const database::Error& error) {
    if (error.kind() == database::ErrorKind::constraint && error.native_code() == 1062 && !error.outcome_unknown()) {
        fail(RepositoryErrorKind::duplicate);
    }
}
}
RepositoryError::RepositoryError(RepositoryErrorKind kind)
    : std::runtime_error([kind] {
        switch (kind) {
        case RepositoryErrorKind::invalid_account: return "Invalid account repository input";
        case RepositoryErrorKind::duplicate: return "Account already exists";
        case RepositoryErrorKind::stale: return "Account snapshot is stale";
        case RepositoryErrorKind::invalid_record: return "Invalid stored account metadata";
        case RepositoryErrorKind::schema: return "Account repository requires current database migrations";
        }
        return "Account repository failure";
    }()), kind_(kind) {}
void AccountRepository::require_schema() {
    const auto state = database::inspect_migrations(connection_);
    if (state.dirty || state.pending || state.current_version < 3) { fail(RepositoryErrorKind::schema); }
    const auto engine = connection_.query(
        "SELECT ENGINE FROM information_schema.tables WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_accounts'",
        {},{1,1,64,64});
    if (engine.rows.size() != 1 || engine.rows[0][0] != "InnoDB") { fail(RepositoryErrorKind::schema); }
    const auto columns = connection_.query(
        "SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE,CHARACTER_MAXIMUM_LENGTH,EXTRA "
        "FROM information_schema.columns WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_accounts' ORDER BY ORDINAL_POSITION",
        {},{9,6,128,4096});
    constexpr std::array<std::string_view,8> names{
        "id","name","role","status","created_at","updated_at","last_login_at","revision"};
    if (columns.rows.size() != names.size()) { fail(RepositoryErrorKind::schema); }
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto& row = columns.rows[i];
        if (row[0] != names[i] || row[3] != (i == 6 ? "YES" : "NO") ||
            row[5] != (i == 0 ? "auto_increment" : "")) { fail(RepositoryErrorKind::schema); }
        if (i >= 1 && i <= 3) {
            if (row[1] != "varbinary" || row[4] != (i == 1 ? "64" : "16")) { fail(RepositoryErrorKind::schema); }
        } else if (row[1] != "bigint" || !row[2] || row[2]->find("unsigned") != std::string::npos) {
            fail(RepositoryErrorKind::schema);
        }
    }
    const auto indexes = connection_.query(
        "SELECT INDEX_NAME,COLUMN_NAME,NON_UNIQUE,SEQ_IN_INDEX,SUB_PART FROM information_schema.statistics "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_accounts' AND INDEX_NAME IN ('PRIMARY','dc24h_accounts_name') ORDER BY INDEX_NAME",
        {},{3,5,64,512});
    if (indexes.rows.size() != 2) { fail(RepositoryErrorKind::schema); }
    bool primary = false, unique_name = false;
    for (const auto& row : indexes.rows) {
        if (row[2] != "0" || row[3] != "1" || row[4]) { fail(RepositoryErrorKind::schema); }
        primary = primary || (row[0] == "PRIMARY" && row[1] == "id");
        unique_name = unique_name || (row[0] == "dc24h_accounts_name" && row[1] == "name");
    }
    if (!primary || !unique_name) { fail(RepositoryErrorKind::schema); }
}
std::optional<StoredAccount> AccountRepository::find_by_id(AccountId id) {
    require_schema();
    return decode(connection_.query(by_id,parameters(id.value()),limits));
}
std::optional<StoredAccount> AccountRepository::find_by_name(const AccountName& name) {
    if (!name.valid()) { fail(RepositoryErrorKind::invalid_account); }
    require_schema();
    return decode(connection_.query(by_name,parameters(name.text()),limits));
}
StoredAccount AccountRepository::insert(const NewAccount& input) {
    // Check all metadata before SQL; the placeholder ID is never persisted.
    auto checked = Account::create({1,input.name,input.role,input.status,input.times});
    if (!std::holds_alternative<Account>(checked)) { fail(RepositoryErrorKind::invalid_account); }
    require_schema();
    auto transaction = connection_.transaction();
    database::Execution written;
    try {
        written = connection_.execute_prepared(
            "INSERT INTO dc24h_accounts(name,role,status,created_at,updated_at,last_login_at) VALUES (?,?,?,?,?,?)",
            parameters(input.name,std::string(role_key(input.role)),std::string(status_key(input.status)),
             input.times.created_at,input.times.updated_at,login(input.times)));
    } catch (const database::Error& error) { duplicate(error); throw; }
    if (written.affected_rows != 1) { fail(RepositoryErrorKind::invalid_record); }
    auto saved = Account::create({written.insert_id,input.name,input.role,input.status,input.times});
    if (!std::holds_alternative<Account>(saved)) { fail(RepositoryErrorKind::invalid_record); }
    StoredAccount result{std::get<Account>(std::move(saved)),1};
    transaction.commit(); // No allocating result construction after commit.
    return result;
}
StoredAccount AccountRepository::update(const StoredAccount& expected, const Account& next) {
    const auto& old_times = expected.account.times();
    const auto& times = next.times();
    if (!expected.account.valid() || !next.valid() || expected.revision == 0 || expected.revision >= max_revision ||
        next.id() != expected.account.id() || times.created_at != old_times.created_at ||
        times.updated_at < old_times.updated_at || (old_times.last_login_at &&
        (!times.last_login_at || *times.last_login_at < *old_times.last_login_at))) {
        fail(RepositoryErrorKind::invalid_account);
    }
    // Complete the owning return value before any mutation.
    StoredAccount result{next,expected.revision+1};
    require_schema();
    auto transaction = connection_.transaction();
    const auto current = decode(connection_.query(locked,parameters(next.id().value()),limits));
    if (!current || *current != expected) { fail(RepositoryErrorKind::stale); }
    database::Execution written;
    try {
        written = connection_.execute_prepared(
            "UPDATE dc24h_accounts SET name=?,role=?,status=?,updated_at=?,last_login_at=?,revision=revision+1 WHERE id=? AND revision=?",
            parameters(next.name().text(),std::string(role_key(next.role())),std::string(status_key(next.status())),
             times.updated_at,login(times),next.id().value(),expected.revision));
    } catch (const database::Error& error) { duplicate(error); throw; }
    if (written.affected_rows != 1) { fail(RepositoryErrorKind::stale); }
    transaction.commit();
    return result;
}
} // namespace dc24h::accounts
