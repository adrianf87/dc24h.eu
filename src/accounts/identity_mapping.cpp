#include "dc24h/accounts/identity_mapping.hpp"
#include "dc24h/accounts/repository.hpp"
#include "dc24h/text/public_text.hpp"

#include <array>
#include <charconv>
#include <type_traits>
#include <utility>

namespace dc24h::accounts {
namespace {
constexpr database::QueryLimits limits{1,4,64,192};
constexpr std::string_view select = "SELECT account_id,nickname,cid,created_at FROM dc24h_account_identities ";
[[noreturn]] void fail(MappingErrorKind kind) { throw MappingError(kind); }
bool valid_nickname(std::string_view value) {
    return !value.empty() && value.size() <= max_identity_nickname_bytes && text::valid_public_text(value,true);
}
template<class... T> database::Parameters parameters(T&&... values) {
    database::Parameters result;
    result.reserve(sizeof...(T));
    (result.emplace_back(std::forward<T>(values)),...);
    return result;
}
template<class T> T number(const database::Cell& cell) {
    if (!cell || cell->empty()) { fail(MappingErrorKind::invalid_record); }
    T value{};
    const auto [end,error] = std::from_chars(cell->data(),cell->data()+cell->size(),value);
    if (error != std::errc{} || end != cell->data()+cell->size() || std::to_string(value) != *cell) {
        fail(MappingErrorKind::invalid_record);
    }
    return value;
}
std::optional<StoredIdentity> decode(const database::Result& result) {
    if (result.rows.empty()) { return std::nullopt; }
    if (result.rows.size() != 1 || result.rows[0].size() != 4) { fail(MappingErrorKind::invalid_record); }
    const auto& row = result.rows[0];
    if (!row[1] || !row[2]) { fail(MappingErrorKind::invalid_record); }
    const auto account = AccountId::from_value(number<std::uint64_t>(row[0]));
    auto identity = IdentityKey::create(*row[1],*row[2]);
    const auto time = number<Timestamp>(row[3]);
    if (!account || !identity || time < 0 || time > max_timestamp) { fail(MappingErrorKind::invalid_record); }
    return StoredIdentity{*account,std::move(*identity),time};
}
}
IdentityKey::IdentityKey(IdentityKey&& other) noexcept
    : nickname_(std::move(other.nickname_)), cid_(other.cid_) { other.nickname_.clear(); }
IdentityKey& IdentityKey::operator=(IdentityKey other) noexcept {
    nickname_.swap(other.nickname_);
    std::swap(cid_,other.cid_);
    return *this;
}
std::optional<IdentityKey> IdentityKey::create(std::string_view nickname, std::string_view cid) {
    if (!valid_nickname(nickname)) { return std::nullopt; }
    const auto checked = adc::TigerCid::parse(cid);
    if (!checked) { return std::nullopt; }
    return IdentityKey(nickname,*checked);
}
MappingError::MappingError(MappingErrorKind kind)
    : std::runtime_error([kind] {
        switch (kind) {
        case MappingErrorKind::invalid_identity: return "Invalid account identity mapping input";
        case MappingErrorKind::invalid_time: return "Invalid account identity mapping timestamp";
        case MappingErrorKind::account_missing: return "Identity mapping account does not exist";
        case MappingErrorKind::conflict: return "Account identity mapping already exists";
        case MappingErrorKind::invalid_record: return "Invalid stored account identity mapping";
        case MappingErrorKind::schema: return "Identity mapping requires compatible current storage";
        }
        return "Account identity mapping failed";
    }()), kind_(kind) {}
void IdentityRepository::require_schema() {
    AccountRepository(connection_).require_schema(); // Also checks transaction/thread ownership.
    const auto state = database::inspect_migrations(connection_);
    if (state.current_version < 5 || state.pending || state.dirty) { fail(MappingErrorKind::schema); }
    const auto engine = connection_.query(
        "SELECT ENGINE FROM information_schema.tables WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_identities'",
        {},{1,1,64,64});
    if (engine.rows.size() != 1 || engine.rows[0][0] != "InnoDB") { fail(MappingErrorKind::schema); }
    const auto columns = connection_.query(
        "SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE,CHARACTER_MAXIMUM_LENGTH,EXTRA "
        "FROM information_schema.columns WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_identities' ORDER BY ORDINAL_POSITION",
        {},{5,6,128,2048});
    constexpr std::array<std::string_view,4> names{"account_id","nickname","cid","created_at"};
    if (columns.rows.size() != names.size()) { fail(MappingErrorKind::schema); }
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto& row = columns.rows[i];
        if (row[0] != names[i] || row[3] != "NO" || row[5] != "") { fail(MappingErrorKind::schema); }
        if (i == 1 || i == 2) {
            if (row[1] != "varbinary" || row[4] != (i == 1 ? "64" : "39")) { fail(MappingErrorKind::schema); }
        } else if (row[1] != "bigint" || !row[2] || row[2]->find("unsigned") != std::string::npos) { fail(MappingErrorKind::schema); }
    }
    const auto indexes = connection_.query(
        "SELECT INDEX_NAME,COLUMN_NAME,NON_UNIQUE,SEQ_IN_INDEX,SUB_PART FROM information_schema.statistics "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_identities' "
        "AND INDEX_NAME IN ('PRIMARY','dc24h_identities_nickname','dc24h_identities_cid') ORDER BY INDEX_NAME",
        {},{4,5,64,1024});
    if (indexes.rows.size() != 3) { fail(MappingErrorKind::schema); }
    bool primary = false, nickname = false, cid = false;
    for (const auto& row : indexes.rows) {
        if (row[2] != "0" || row[3] != "1" || row[4]) { fail(MappingErrorKind::schema); }
        primary = primary || (row[0] == "PRIMARY" && row[1] == "account_id");
        nickname = nickname || (row[0] == "dc24h_identities_nickname" && row[1] == "nickname");
        cid = cid || (row[0] == "dc24h_identities_cid" && row[1] == "cid");
    }
    if (!primary || !nickname || !cid) { fail(MappingErrorKind::schema); }
    const auto foreign = connection_.query(
        "SELECT COLUMN_NAME,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME,REFERENCED_TABLE_SCHEMA=DATABASE() "
        "FROM information_schema.key_column_usage WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='dc24h_account_identities' AND CONSTRAINT_NAME='dc24h_identities_account'",
        {},{2,4,64,256});
    if (foreign.rows.size() != 1 || foreign.rows[0][0] != "account_id" || foreign.rows[0][1] != "dc24h_accounts" ||
        foreign.rows[0][2] != "id" || foreign.rows[0][3] != "1") { fail(MappingErrorKind::schema); }
}
StoredIdentity IdentityRepository::bind(AccountId account, const IdentityKey& identity, Timestamp created_at) {
    if (!identity.valid()) { fail(MappingErrorKind::invalid_identity); }
    if (created_at < 0 || created_at > max_timestamp) { fail(MappingErrorKind::invalid_time); }
    StoredIdentity result{account,identity,created_at}; // Allocate before any SQL mutation.
    require_schema();
    auto transaction = connection_.transaction();
    const auto owner = connection_.query("SELECT id FROM dc24h_accounts WHERE id=? FOR UPDATE",
                                         parameters(account.value()),{1,1,20,20});
    if (owner.rows.empty()) { fail(MappingErrorKind::account_missing); }
    try {
        const auto written = connection_.execute_prepared(
            "INSERT INTO dc24h_account_identities(account_id,nickname,cid,created_at) VALUES (?,?,?,?)",
            parameters(account.value(),identity.nickname(),identity.cid().text(),created_at));
        if (written.affected_rows != 1) { fail(MappingErrorKind::invalid_record); }
    } catch (const database::Error& error) {
        if (error.kind() == database::ErrorKind::constraint && error.native_code() == 1062 && !error.outcome_unknown()) {
            fail(MappingErrorKind::conflict);
        }
        throw;
    }
    static_assert(std::is_nothrow_move_constructible_v<StoredIdentity>);
    transaction.commit();
    return result;
}
std::optional<StoredIdentity> IdentityRepository::find_by_account(AccountId account) {
    require_schema();
    return decode(connection_.query(std::string(select)+"WHERE account_id=?",parameters(account.value()),limits));
}
std::optional<StoredIdentity> IdentityRepository::find_by_nickname(std::string_view nickname) {
    if (!valid_nickname(nickname)) { fail(MappingErrorKind::invalid_identity); }
    require_schema();
    return decode(connection_.query(std::string(select)+"WHERE nickname=?",parameters(std::string(nickname)),limits));
}
std::optional<StoredIdentity> IdentityRepository::find_by_cid(const adc::TigerCid& cid) {
    require_schema();
    return decode(connection_.query(std::string(select)+"WHERE cid=?",parameters(cid.text()),limits));
}
std::optional<StoredIdentity> IdentityRepository::resolve(const IdentityKey& identity) {
    if (!identity.valid()) { fail(MappingErrorKind::invalid_identity); }
    require_schema();
    return decode(connection_.query(std::string(select)+"WHERE nickname=? AND cid=?",
                                   parameters(identity.nickname(),identity.cid().text()),limits));
}
} // namespace dc24h::accounts
