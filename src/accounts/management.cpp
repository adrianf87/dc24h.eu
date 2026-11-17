#include "dc24h/accounts/management.hpp"
#include "dc24h/accounts/adc_authentication.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include "dc24h/security/roles.hpp"
#include "registration_internal.hpp"
#include "adc_storage_internal.hpp"

#include <openssl/crypto.h>
#include <charconv>
#include <limits>
#include <type_traits>
#include <utility>

namespace dc24h::accounts {
namespace {
[[noreturn]] void fail(ManagementErrorKind kind) { throw ManagementError(kind); }
template<class... T> database::Parameters parameters(T&&... values) {
    database::Parameters result; result.reserve(sizeof...(T));
    (result.emplace_back(std::forward<T>(values)),...); return result;
}
struct EraseResult {
    database::Result& result;
    ~EraseResult() { for (auto& row : result.rows) { for (auto& cell : row) {
        if (cell && !cell->empty()) { OPENSSL_cleanse(cell->data(),cell->size()); }
    } } }
};
struct EraseParameters {
    database::Parameters& parameters;
    ~EraseParameters() { for (auto& item : parameters) {
        if (auto* value = std::get_if<database::Bytes>(&item); value && !value->empty()) {
            OPENSSL_cleanse(value->data(),value->size());
        }
    } }
};
void valid_snapshot(const StoredAccount& value) {
    if (!value.account.valid() || value.revision == 0 ||
        value.revision >= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) { fail(ManagementErrorKind::invalid_input); }
}
void valid_credential(const database::Result& result, const StoredAccount& owner) {
    if (result.rows.size() != 1 || result.rows[0].size() != 2 || !result.rows[0][0] || !result.rows[0][1]) {
        fail(ManagementErrorKind::invalid_record);
    }
    const auto& encoded = *result.rows[0][1]; Timestamp time{};
    const auto [end,error] = std::from_chars(encoded.data(),encoded.data()+encoded.size(),time);
    if (error != std::errc{} || end != encoded.data()+encoded.size() || std::to_string(time) != encoded ||
        time < owner.account.times().created_at || time > owner.account.times().updated_at) { fail(ManagementErrorKind::invalid_record); }
}
}
ManagementError::ManagementError(ManagementErrorKind kind) : std::runtime_error([kind] {
    switch (kind) {
    case ManagementErrorKind::invalid_input: return "Invalid account management input";
    case ManagementErrorKind::invalid_password: return "Invalid account management password";
    case ManagementErrorKind::unauthorized: return "Account management authorization failed";
    case ManagementErrorKind::stale: return "Account management snapshot is stale";
    case ManagementErrorKind::invalid_record: return "Invalid stored account management credential";
    case ManagementErrorKind::crypto: return "Account management cryptography failed";
    case ManagementErrorKind::schema: return "Account management requires compatible current storage";
    }
    return "Account management failed";
}()),kind_(kind) {}
StoredAccount AccountManagementService::create_account(const StoredAccount& actor,
    std::string_view proof,std::string_view name,std::string_view password,Timestamp time) {
    valid_snapshot(actor);
    if(actor.account.role()!=Role::administrator||actor.account.status()!=Status::active){fail(ManagementErrorKind::unauthorized);}
    if(time<actor.account.times().updated_at||time>max_timestamp){fail(ManagementErrorKind::invalid_input);}
    const auto checked_name=AccountName::create(name);
    if(!checked_name){throw RegistrationError(RegistrationErrorKind::invalid_name);}
    detail::validate_password(proof);detail::validate_password(password);
    AccountRepository repository(connection_);repository.require_schema();
    RegistrationService(connection_).require_credentials_schema();
    if(repository.find_by_id(actor.account.id())!=actor){fail(ManagementErrorKind::stale);}
    const auto key=parameters(actor.account.id().value());
    auto captured=connection_.query("SELECT verifier,created_at FROM dc24h_account_credentials WHERE account_id=?",key,{1,2,192,256});
    EraseResult erase_captured{captured};valid_credential(captured,actor);
    if(!detail::verify_password(proof,*captured.rows[0][0])){fail(ManagementErrorKind::unauthorized);}
    detail::EncodedPassword verifier;detail::hash_password(password,verifier);
    auto transaction=connection_.transaction();
    if(repository.locked_in_transaction(actor.account.id())!=actor){fail(ManagementErrorKind::stale);}
    auto current=connection_.query("SELECT verifier,created_at FROM dc24h_account_credentials WHERE account_id=? FOR UPDATE",key,{1,2,192,256});
    EraseResult erase_current{current};valid_credential(current,actor);
    if(current.rows!=captured.rows){fail(ManagementErrorKind::stale);}
    auto result=repository.insert_in_transaction({checked_name->text(),Role::registered,Status::disabled,{time,time,{}}});
    database::Parameters bound;EraseParameters erase_bound{bound};bound.reserve(3);
    bound.emplace_back(result.account.id().value());
    bound.emplace_back(database::Bytes(verifier.bytes.begin(),verifier.bytes.end()));bound.emplace_back(time);
    if(connection_.execute_prepared("INSERT INTO dc24h_account_credentials(account_id,verifier,created_at) VALUES (?,?,?)",bound).affected_rows!=1){
        fail(ManagementErrorKind::invalid_record);
    }
    transaction.commit();return result;
}
StoredAccount AccountManagementService::change_password(const StoredAccount& expected,
    std::string_view current_password,std::string_view new_password,Timestamp time) {
    return perform(Action::change,expected,current_password,expected,new_password,time);
}
StoredAccount AccountManagementService::disable_account(const StoredAccount& actor,
    std::string_view actor_password,const StoredAccount& expected,Timestamp time) {
    return perform(Action::disable,actor,actor_password,expected,{},time);
}
StoredAccount AccountManagementService::reset_password(const StoredAccount& actor,
    std::string_view actor_password,const StoredAccount& expected,std::string_view new_password,Timestamp time) {
    return perform(Action::reset,actor,actor_password,expected,new_password,time);
}
StoredAccount AccountManagementService::activate_account(const StoredAccount& actor,
    std::string_view actor_password,const StoredAccount& expected,Timestamp time) {
    return perform(Action::activate,actor,actor_password,expected,{},time);
}
StoredAccount AccountManagementService::perform(Action action,const StoredAccount& actor,
    std::string_view proof,const StoredAccount& target,std::string_view replacement,Timestamp time) {
    try {
        valid_snapshot(actor); valid_snapshot(target);
        if (actor.account.status() != Status::active ||
            (action != Action::change && (actor.account.id() == target.account.id() ||
             !security::can_manage(actor.account.role(),target.account.role())))) { fail(ManagementErrorKind::unauthorized); }
        if (time < actor.account.times().updated_at || time < target.account.times().updated_at || time > max_timestamp) {
            fail(ManagementErrorKind::invalid_input);
        }
        detail::validate_password(proof);
        const bool changes_password = action == Action::change || action == Action::reset;
        if (changes_password) { detail::validate_password(replacement); }
        if (action == Action::change && proof == replacement) { fail(ManagementErrorKind::invalid_password); }
        const auto next = std::get<Account>(target.account.with_access(target.account.role(),
            action == Action::change ? target.account.status() : action == Action::activate ? Status::active : Status::disabled,time));
        AccountRepository repository(connection_);
        repository.require_schema();
        RegistrationService(connection_).require_credentials_schema();
        detail::require_adc_credentials_schema(connection_);
        if (repository.find_by_id(actor.account.id()) != actor || repository.find_by_id(target.account.id()) != target) {
            fail(ManagementErrorKind::stale);
        }
        const auto key = parameters(actor.account.id().value());
        auto captured = connection_.query("SELECT verifier,created_at FROM dc24h_account_credentials WHERE account_id=?",key,{1,2,192,256});
        EraseResult erase_captured{captured}; valid_credential(captured,actor);
        if (!detail::verify_password(proof,*captured.rows[0][0])) { fail(ManagementErrorKind::unauthorized); }
        detail::EncodedPassword verifier;
        if (changes_password) { detail::hash_password(replacement,verifier); }
        // All expensive KDF work precedes locks. The exact proof record and both
        // account snapshots are checked again in the owning transaction.
        auto transaction = connection_.transaction();
        const auto lock = [&](const StoredAccount& expected) {
            if (repository.locked_in_transaction(expected.account.id()) != expected) { fail(ManagementErrorKind::stale); }
        };
        if (actor.account.id().value() < target.account.id().value()) { lock(actor); lock(target); }
        else { lock(target); if (actor.account.id() != target.account.id()) { lock(actor); } }
        auto current = connection_.query("SELECT verifier,created_at FROM dc24h_account_credentials WHERE account_id=? FOR UPDATE",key,{1,2,192,256});
        EraseResult erase_current{current}; valid_credential(current,actor);
        if (current.rows != captured.rows) { fail(ManagementErrorKind::stale); }
        auto result = repository.update_in_transaction(target,next);
        if (changes_password) {
            database::Parameters bound; EraseParameters erase_bound{bound}; bound.reserve(3);
            bound.emplace_back(target.account.id().value());
            bound.emplace_back(database::Bytes(verifier.bytes.begin(),verifier.bytes.end()));
            bound.emplace_back(time);
            const auto written = connection_.execute_prepared(
                "INSERT INTO dc24h_account_credentials(account_id,verifier,created_at) VALUES (?,?,?) "
                "ON DUPLICATE KEY UPDATE verifier=VALUES(verifier),created_at=VALUES(created_at)",bound);
            if (written.affected_rows < 1 || written.affected_rows > 2) { fail(ManagementErrorKind::invalid_record); }
        }
        if (connection_.execute_prepared("DELETE FROM dc24h_account_adc_credentials WHERE account_id=?",
            parameters(target.account.id().value())).affected_rows > 1) { fail(ManagementErrorKind::invalid_record); }
        static_assert(std::is_nothrow_move_constructible_v<StoredAccount>);
        transaction.commit();
        return result;
    } catch (const RegistrationError& error) {
        fail(error.kind() == RegistrationErrorKind::crypto ? ManagementErrorKind::crypto :
             error.kind() == RegistrationErrorKind::invalid_password ? ManagementErrorKind::invalid_password : ManagementErrorKind::schema);
    } catch (const PasswordSecurityError& error) {
        fail(error.kind() == PasswordSecurityErrorKind::invalid_record ? ManagementErrorKind::invalid_record : ManagementErrorKind::crypto);
    } catch (const AdcCredentialError&) { fail(ManagementErrorKind::schema);
    } catch (const RepositoryError& error) {
        fail(error.kind() == RepositoryErrorKind::stale ? ManagementErrorKind::stale :
             error.kind() == RepositoryErrorKind::schema ? ManagementErrorKind::schema : ManagementErrorKind::invalid_record);
    }
}
} // namespace dc24h::accounts
