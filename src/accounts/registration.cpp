#include "dc24h/accounts/registration.hpp"
#include "registration_internal.hpp"

#include <openssl/crypto.h>

#include <array>
#include <type_traits>

namespace dc24h::accounts {
namespace {
[[noreturn]] void schema() { throw RegistrationError(RegistrationErrorKind::schema); }
}
RegistrationError::RegistrationError(RegistrationErrorKind kind)
    : std::runtime_error([kind] {
        switch (kind) {
        case RegistrationErrorKind::invalid_name: return "Invalid registration account name";
        case RegistrationErrorKind::invalid_password: return "Invalid registration password";
        case RegistrationErrorKind::invalid_time: return "Invalid registration timestamp";
        case RegistrationErrorKind::crypto: return "Registration password derivation failed";
        case RegistrationErrorKind::schema: return "Registration requires compatible credential storage";
        }
        return "Account registration failed";
    }()), kind_(kind) {}
void RegistrationService::require_credentials_schema() {
    const auto engine = connection_.query(
        "SELECT ENGINE FROM information_schema.tables WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_credentials'",
        {},{1,1,64,64});
    if (engine.rows.size() != 1 || engine.rows[0][0] != "InnoDB") { schema(); }
    const auto columns = connection_.query(
        "SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE,CHARACTER_MAXIMUM_LENGTH,EXTRA "
        "FROM information_schema.columns WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_credentials' ORDER BY ORDINAL_POSITION",
        {},{4,6,128,2048});
    constexpr std::array<std::string_view,3> names{"account_id","verifier","created_at"};
    if (columns.rows.size() != names.size()) { schema(); }
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto& row = columns.rows[i];
        if (row[0] != names[i] || row[3] != "NO" || row[5] != "") { schema(); }
        if (i == 1) {
            if (row[1] != "varbinary" || row[4] != "192") { schema(); }
        } else if (row[1] != "bigint" || !row[2] || row[2]->find("unsigned") != std::string::npos) { schema(); }
    }
    const auto key = connection_.query(
        "SELECT COLUMN_NAME,NON_UNIQUE,SEQ_IN_INDEX,SUB_PART FROM information_schema.statistics "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_credentials' AND INDEX_NAME='PRIMARY'",
        {},{2,4,64,256});
    if (key.rows.size() != 1 || key.rows[0][0] != "account_id" || key.rows[0][1] != "0" ||
        key.rows[0][2] != "1" || key.rows[0][3]) { schema(); }
    const auto foreign = connection_.query(
        "SELECT COLUMN_NAME,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME,REFERENCED_TABLE_SCHEMA=DATABASE() FROM information_schema.key_column_usage "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_credentials' AND CONSTRAINT_NAME='dc24h_credentials_account'",
        {},{2,4,64,256});
    if (foreign.rows.size() != 1 || foreign.rows[0][0] != "account_id" ||
        foreign.rows[0][1] != "dc24h_accounts" || foreign.rows[0][2] != "id" || foreign.rows[0][3] != "1") { schema(); }
}
StoredAccount RegistrationService::create_account(std::string_view name, std::string_view password, Timestamp created_at) {
    if (created_at < 0 || created_at > max_timestamp) { throw RegistrationError(RegistrationErrorKind::invalid_time); }
    const auto checked_name = AccountName::create(name);
    if (!checked_name) { throw RegistrationError(RegistrationErrorKind::invalid_name); }
    detail::validate_password(password);
    AccountRepository repository(connection_);
    repository.require_schema(); // Also refuses caller transactions/wrong threads.
    require_credentials_schema();
    detail::EncodedPassword verifier;
    detail::hash_password(password,verifier); // Expensive work precedes transaction/row locks.
    NewAccount input{checked_name->text(),Role::registered,Status::disabled,{created_at,created_at,{}}};
    auto transaction = connection_.transaction();
    auto result = repository.insert_in_transaction(input);
    // The only application-owned heap copy of the encoded verifier is wiped.
    database::Parameters bound;
    bound.reserve(3);
    bound.emplace_back(result.account.id().value());
    bound.emplace_back(database::Bytes(verifier.bytes.begin(),verifier.bytes.end()));
    struct Erase {
        database::Parameters& bound;
        ~Erase() { auto& value = std::get<database::Bytes>(bound[1]); OPENSSL_cleanse(value.data(),value.size()); }
    } erase{bound};
    bound.emplace_back(created_at);
    const auto written = connection_.execute_prepared(
        "INSERT INTO dc24h_account_credentials(account_id,verifier,created_at) VALUES (?,?,?)",bound);
    if (written.affected_rows != 1) { schema(); }
    static_assert(std::is_nothrow_move_constructible_v<StoredAccount>);
    transaction.commit();
    return result;
}
} // namespace dc24h::accounts
