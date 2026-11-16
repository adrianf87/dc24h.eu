#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/repository.hpp"
#include "dc24h/accounts/registration.hpp"
#include "registration_internal.hpp"

#include <openssl/crypto.h>

namespace dc24h::accounts {
namespace {
[[noreturn]] void translate(const RegistrationError& error) {
    switch (error.kind()) {
    case RegistrationErrorKind::invalid_password:
        throw PasswordSecurityError(PasswordSecurityErrorKind::invalid_password);
    case RegistrationErrorKind::crypto:
        throw PasswordSecurityError(PasswordSecurityErrorKind::crypto);
    default:
        throw PasswordSecurityError(PasswordSecurityErrorKind::schema);
    }
}
// Do not copy credential cells. Wipe owned application buffers on every exit,
// including malformed records and KDF/provider failures.
struct EraseResult final {
    database::Result& result;
    ~EraseResult() {
        for (auto& row : result.rows) {
            for (auto& cell : row) {
                if (cell && !cell->empty()) { OPENSSL_cleanse(cell->data(),cell->size()); }
            }
        }
    }
};
}
PasswordSecurityError::PasswordSecurityError(PasswordSecurityErrorKind kind)
    : std::runtime_error([kind] {
        switch (kind) {
        case PasswordSecurityErrorKind::invalid_password: return "Invalid credential check password";
        case PasswordSecurityErrorKind::invalid_record: return "Invalid stored password verifier";
        case PasswordSecurityErrorKind::crypto: return "Password verification failed";
        case PasswordSecurityErrorKind::schema: return "Password verification requires compatible credential storage";
        }
        return "Credential check failed";
    }()), kind_(kind) {}
PasswordVerification PasswordSecurityService::verify(AccountId account, std::string_view password) {
    try { detail::validate_password(password); }
    catch (const RegistrationError& error) { translate(error); }
    // Both checks are read-only and reject stale/dirty schemas, transactions
    // owned by callers and connections used from the wrong thread.
    AccountRepository(connection_).require_schema();
    try { RegistrationService(connection_).require_credentials_schema(); }
    catch (const RegistrationError& error) { translate(error); }
    database::Parameters bound;
    bound.emplace_back(account.value());
    auto result = connection_.query(
        "SELECT verifier FROM dc24h_account_credentials WHERE account_id=?",bound,{1,1,192,192});
    EraseResult erase{result};
    if (result.rows.empty()) { return PasswordVerification::unavailable; }
    if (result.rows.size() != 1 || result.rows[0].size() != 1 || !result.rows[0][0]) {
        throw PasswordSecurityError(PasswordSecurityErrorKind::invalid_record);
    }
    try {
        return detail::verify_password(password,*result.rows[0][0])
            ? PasswordVerification::matched : PasswordVerification::mismatch;
    } catch (const RegistrationError& error) { translate(error); }
}
} // namespace dc24h::accounts
