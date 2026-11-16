#include "dc24h/accounts/adc_authentication.hpp"
#include "dc24h/accounts/identity_mapping.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include "dc24h/accounts/repository.hpp"
#include "dc24h/security/secret_file.hpp"
#include "registration_internal.hpp"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <algorithm>
#include <charconv>
#include <unordered_map>

namespace dc24h::accounts {
namespace {
constexpr std::string_view magic = "DC24HADC1";
constexpr std::size_t nonce_bytes = 12, tag_bytes = 16, max_envelope = 1100;
[[noreturn]] void fail(AdcCredentialErrorKind kind) { throw AdcCredentialError(kind); }
struct PlainSecret {
    std::array<unsigned char,1024+EVP_MAX_BLOCK_LENGTH> bytes{};
    std::size_t size{};
    PlainSecret() = default;
    PlainSecret(const PlainSecret&) = delete;
    ~PlainSecret() { OPENSSL_cleanse(bytes.data(),bytes.size()); }
    std::string_view view() const noexcept { return {reinterpret_cast<const char*>(bytes.data()),size}; }
};
struct EraseString { std::string& value; ~EraseString() { if (!value.empty()) { OPENSSL_cleanse(value.data(),value.size()); } } };
template<class... T> database::Parameters parameters(T&&... values) {
    database::Parameters result; result.reserve(sizeof...(T));
    (result.emplace_back(std::forward<T>(values)),...); return result;
}
template<class T> T number(const database::Cell& cell) {
    if (!cell || cell->empty()) { fail(AdcCredentialErrorKind::invalid_record); }
    T value{}; const auto [end,error] = std::from_chars(cell->data(),cell->data()+cell->size(),value);
    if (error != std::errc{} || end != cell->data()+cell->size() || std::to_string(value) != *cell) { fail(AdcCredentialErrorKind::invalid_record); }
    return value;
}
std::array<unsigned char,magic.size()+16> aad(AccountId account,Timestamp time) noexcept {
    std::array<unsigned char,magic.size()+16> bytes{};
    std::copy(magic.begin(),magic.end(),bytes.begin());
    const auto id = account.value(), seconds = static_cast<std::uint64_t>(time);
    for (unsigned i = 0; i < 8; ++i) {
        bytes[magic.size()+i] = static_cast<unsigned char>(id >> ((7-i)*8));
        bytes[magic.size()+8+i] = static_cast<unsigned char>(seconds >> ((7-i)*8));
    }
    return bytes;
}
using Context = std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)>;
std::string seal(const std::array<unsigned char,32>& key,AccountId account,Timestamp time,std::string_view password) {
    std::string output(magic.size()+nonce_bytes+password.size()+tag_bytes,'\0');
    std::copy(magic.begin(),magic.end(),output.begin());
    auto* raw = reinterpret_cast<unsigned char*>(output.data());
    auto* nonce = raw+magic.size(); auto* cipher = nonce+nonce_bytes;
    Context context(EVP_CIPHER_CTX_new(),&EVP_CIPHER_CTX_free);
    const auto associated = aad(account,time); int count = 0, final = 0;
    if (!context || RAND_bytes(nonce,static_cast<int>(nonce_bytes)) != 1 ||
        EVP_EncryptInit_ex(context.get(),EVP_aes_256_gcm(),nullptr,key.data(),nonce) != 1 ||
        EVP_EncryptUpdate(context.get(),nullptr,&count,associated.data(),static_cast<int>(associated.size())) != 1 ||
        EVP_EncryptUpdate(context.get(),cipher,&count,reinterpret_cast<const unsigned char*>(password.data()),static_cast<int>(password.size())) != 1 ||
        count != static_cast<int>(password.size()) || EVP_EncryptFinal_ex(context.get(),cipher+count,&final) != 1 || final != 0 ||
        EVP_CIPHER_CTX_ctrl(context.get(),EVP_CTRL_GCM_GET_TAG,static_cast<int>(tag_bytes),cipher+password.size()) != 1) {
        OPENSSL_cleanse(output.data(),output.size()); fail(AdcCredentialErrorKind::crypto);
    }
    return output;
}
void open(const std::array<unsigned char,32>& key,AccountId account,Timestamp time,std::string_view record,PlainSecret& output) {
    if (!record.starts_with(magic) || record.size() <= magic.size()+nonce_bytes+tag_bytes ||
        record.size() > magic.size()+nonce_bytes+tag_bytes+1024 || time < 0 || time > max_timestamp) {
        fail(AdcCredentialErrorKind::invalid_record);
    }
    const auto size = record.size()-magic.size()-nonce_bytes-tag_bytes;
    const auto* raw = reinterpret_cast<const unsigned char*>(record.data());
    const auto* nonce = raw+magic.size(); const auto* cipher = nonce+nonce_bytes;
    std::array<unsigned char,tag_bytes> tag{};
    std::copy_n(cipher+size,tag.size(),tag.begin());
    const auto associated = aad(account,time); Context context(EVP_CIPHER_CTX_new(),&EVP_CIPHER_CTX_free);
    int count = 0, final = 0;
    if (!context || EVP_DecryptInit_ex(context.get(),EVP_aes_256_gcm(),nullptr,key.data(),nonce) != 1 ||
        EVP_DecryptUpdate(context.get(),nullptr,&count,associated.data(),static_cast<int>(associated.size())) != 1 ||
        EVP_DecryptUpdate(context.get(),output.bytes.data(),&count,cipher,static_cast<int>(size)) != 1 || count != static_cast<int>(size) ||
        EVP_CIPHER_CTX_ctrl(context.get(),EVP_CTRL_GCM_SET_TAG,static_cast<int>(tag.size()),tag.data()) != 1 ||
        EVP_DecryptFinal_ex(context.get(),output.bytes.data()+count,&final) != 1 || final != 0) {
        fail(AdcCredentialErrorKind::crypto);
    }
    output.size = size;
    try { detail::validate_password(output.view()); }
    catch (const RegistrationError&) { fail(AdcCredentialErrorKind::invalid_record); }
}
void require_storage(database::Connection& connection) {
    const auto status = database::inspect_migrations(connection);
    if (status.dirty || status.pending || status.current_version < 6) { fail(AdcCredentialErrorKind::schema); }
    const auto engine = connection.query("SELECT ENGINE FROM information_schema.tables WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_adc_credentials'",{},{1,1,64,64});
    if (engine.rows.size() != 1 || engine.rows[0][0] != "InnoDB") { fail(AdcCredentialErrorKind::schema); }
    const auto columns = connection.query("SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE,CHARACTER_MAXIMUM_LENGTH,EXTRA FROM information_schema.columns WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_adc_credentials' ORDER BY ORDINAL_POSITION",{},{4,6,128,2048});
    constexpr std::array<std::string_view,3> names{"account_id","secret","created_at"};
    if (columns.rows.size() != names.size()) { fail(AdcCredentialErrorKind::schema); }
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto& row = columns.rows[i];
        if (row[0] != names[i] || row[3] != "NO" || row[5] != "") { fail(AdcCredentialErrorKind::schema); }
        if (i == 1) {
            if (row[1] != "varbinary" || row[4] != "1100") { fail(AdcCredentialErrorKind::schema); }
        } else if (row[1] != "bigint" || !row[2] || row[2]->find("unsigned") != std::string::npos) { fail(AdcCredentialErrorKind::schema); }
    }
    const auto keys = connection.query("SELECT COLUMN_NAME,NON_UNIQUE,SEQ_IN_INDEX,SUB_PART FROM information_schema.statistics WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_adc_credentials' AND INDEX_NAME='PRIMARY'",{},{2,4,64,256});
    if (keys.rows.size() != 1 || keys.rows[0][0] != "account_id" || keys.rows[0][1] != "0" || keys.rows[0][2] != "1" || keys.rows[0][3]) { fail(AdcCredentialErrorKind::schema); }
    const auto fk = connection.query("SELECT COLUMN_NAME,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME,REFERENCED_TABLE_SCHEMA=DATABASE() FROM information_schema.key_column_usage WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_account_adc_credentials' AND CONSTRAINT_NAME='dc24h_adc_credentials_account'",{},{2,4,64,256});
    if (fk.rows.size() != 1 || fk.rows[0][0] != "account_id" || fk.rows[0][1] != "dc24h_accounts" || fk.rows[0][2] != "id" || fk.rows[0][3] != "1") { fail(AdcCredentialErrorKind::schema); }
}
}
AdcCredentialError::AdcCredentialError(AdcCredentialErrorKind kind) : std::runtime_error([kind] {
    switch (kind) {
    case AdcCredentialErrorKind::input: return "Invalid ADC credential input";
    case AdcCredentialErrorKind::proof: return "ADC credential enrollment proof failed";
    case AdcCredentialErrorKind::conflict: return "ADC credential already enrolled";
    case AdcCredentialErrorKind::mapping: return "ADC credential requires an account identity mapping";
    case AdcCredentialErrorKind::schema: return "ADC authentication requires compatible current storage";
    case AdcCredentialErrorKind::invalid_record: return "Invalid stored ADC credential or identity";
    case AdcCredentialErrorKind::crypto: return "ADC credential cryptography failed";
    }
    return "ADC authentication failed";
}()),kind_(kind) {}
AdcSecretKey::~AdcSecretKey() { OPENSSL_cleanse(bytes_.data(),bytes_.size()); }
std::shared_ptr<const AdcSecretKey> AdcSecretKey::load(const std::string& path) {
    auto encoded = security::read_private_secret(path); EraseString erase{encoded};
    if (encoded.size() != 64) { fail(AdcCredentialErrorKind::input); }
    auto key = std::shared_ptr<AdcSecretKey>(new AdcSecretKey);
    const auto digit = [](char value) -> unsigned char {
        if (value >= '0' && value <= '9') { return static_cast<unsigned char>(value-'0'); }
        if (value >= 'a' && value <= 'f') { return static_cast<unsigned char>(value-'a'+10); }
        fail(AdcCredentialErrorKind::input);
    };
    for (std::size_t i = 0; i < key->bytes_.size(); ++i) { key->bytes_[i] = static_cast<unsigned char>((digit(encoded[i*2]) << 4) | digit(encoded[i*2+1])); }
    return key;
}
void AdcCredentialService::enroll(AccountId account,std::string_view account_password,std::string_view adc_password,Timestamp time) {
    if (time < 0 || time > max_timestamp) { fail(AdcCredentialErrorKind::input); }
    detail::validate_password(adc_password);
    if (PasswordSecurityService(connection_).verify(account,account_password) != PasswordVerification::matched) { fail(AdcCredentialErrorKind::proof); }
    require_storage(connection_);
    const auto stored = AccountRepository(connection_).find_by_id(account);
    if (!stored || time < stored->account.times().created_at) { fail(AdcCredentialErrorKind::input); }
    if (!IdentityRepository(connection_).find_by_account(account)) { fail(AdcCredentialErrorKind::mapping); }
    auto encrypted = seal(key_.bytes_,account,time,adc_password); EraseString erase{encrypted};
    auto transaction = connection_.transaction();
    if (connection_.query("SELECT id FROM dc24h_accounts WHERE id=? FOR UPDATE",parameters(account.value()),{1,1,20,20}).rows.empty()) { fail(AdcCredentialErrorKind::input); }
    try {
        const auto written = connection_.execute_prepared("INSERT INTO dc24h_account_adc_credentials(account_id,secret,created_at) VALUES (?,?,?)",
            parameters(account.value(),database::Bytes(encrypted.begin(),encrypted.end()),time));
        if (written.affected_rows != 1) { fail(AdcCredentialErrorKind::invalid_record); }
    } catch (const database::Error& error) {
        if (error.kind() == database::ErrorKind::constraint && error.native_code() == 1062 && !error.outcome_unknown()) { fail(AdcCredentialErrorKind::conflict); }
        throw;
    }
    transaction.commit();
}
struct AdcAuthenticationDirectory::Impl {
    struct Record { AccountId id; IdentityKey identity; Role role; Status status; std::optional<std::string> secret; Timestamp created_at; };
    std::shared_ptr<const AdcSecretKey> key;
    bool require_tls;
    std::vector<Record> records;
    // Transparent allocation-free lookups on the reactor.
    struct Hash { using is_transparent = void; std::size_t operator()(std::string_view value) const noexcept { return std::hash<std::string_view>{}(value); } };
    std::unordered_map<std::string,std::size_t,Hash,std::equal_to<>> nicknames,cids;
};
AdcAuthenticationDirectory::AdcAuthenticationDirectory(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
AdcAuthenticationDirectory::~AdcAuthenticationDirectory() = default;
std::shared_ptr<const AdcAuthenticationDirectory> AdcAuthenticationDirectory::load(database::Connection& connection,
    std::shared_ptr<const AdcSecretKey> key,std::size_t max_accounts,bool require_tls) {
    if (!key || max_accounts == 0 || max_accounts > 4096) { fail(AdcCredentialErrorKind::input); }
    IdentityRepository(connection).require_schema();
    require_storage(connection);
    auto impl = std::make_unique<Impl>(); impl->key = std::move(key); impl->require_tls = require_tls;
    (void)connection.execute_prepared("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    auto transaction = connection.transaction(); std::uint64_t after = 0;
    for (;;) {
        const auto result = connection.query(
            "SELECT a.id,a.name,a.role,a.status,a.created_at,a.updated_at,a.last_login_at,i.nickname,i.cid,c.secret,c.created_at AS credential_created_at "
            "FROM dc24h_account_identities i JOIN dc24h_accounts a ON a.id=i.account_id LEFT JOIN dc24h_account_adc_credentials c ON c.account_id=a.id WHERE a.id>? ORDER BY a.id LIMIT 64",
            parameters(after),{64,11,max_envelope,131072});
        if (result.rows.empty()) { break; }
        for (const auto& row : result.rows) {
            if (impl->records.size() == max_accounts || row.size() != 11 || !row[1] || !row[2] || !row[3] || !row[7] || !row[8]) { fail(AdcCredentialErrorKind::invalid_record); }
            const auto id = number<std::uint64_t>(row[0]); const auto role = parse_role(*row[2]); const auto status = parse_status(*row[3]);
            if (id <= after || !role || !status) { fail(AdcCredentialErrorKind::invalid_record); }
            Times times{number<Timestamp>(row[4]),number<Timestamp>(row[5]),{}}; if (row[6]) { times.last_login_at = number<Timestamp>(row[6]); }
            const auto account = Account::create({id,*row[1],*role,*status,times});
            auto identity = IdentityKey::create(*row[7],*row[8]);
            if (!std::holds_alternative<Account>(account) || !identity || row[9].has_value() != row[10].has_value()) { fail(AdcCredentialErrorKind::invalid_record); }
            const auto time = row[10] ? number<Timestamp>(row[10]) : Timestamp{0};
            if (row[9] && time < times.created_at) { fail(AdcCredentialErrorKind::invalid_record); }
            if (row[9]) { PlainSecret plain; open(impl->key->bytes_,*AccountId::from_value(id),time,*row[9],plain); }
            const auto index = impl->records.size();
            if (!impl->nicknames.emplace(identity->nickname(),index).second || !impl->cids.emplace(identity->cid().text(),index).second) { fail(AdcCredentialErrorKind::invalid_record); }
            impl->records.push_back({*AccountId::from_value(id),std::move(*identity),*role,*status,row[9],time}); after = id;
        }
    }
    transaction.commit();
    return std::shared_ptr<const AdcAuthenticationDirectory>(new AdcAuthenticationDirectory(std::move(impl)));
}
adc::AuthenticationPreparation AdcAuthenticationDirectory::prepare(std::string_view nickname,std::string_view cid,
    users::Transport transport,adc::PasswordChallenge& challenge) const {
    const auto nick = impl_->nicknames.find(nickname), client = impl_->cids.find(cid);
    if (nick == impl_->nicknames.end() && client == impl_->cids.end()) { return adc::GuestAuthentication{}; }
    if (nick == impl_->nicknames.end() || client == impl_->cids.end() || nick->second != client->second) { return adc::DeniedAuthentication{}; }
    const auto& record = impl_->records[nick->second];
    if (record.status != Status::active || !record.secret || (impl_->require_tls && transport != users::Transport::adcs)) { return adc::DeniedAuthentication{}; }
    PlainSecret plain; open(impl_->key->bytes_,record.id,record.created_at,*record.secret,plain);
    auto prepared = challenge.issue(plain.view(),adc::SessionHash::tiger192);
    if (!std::holds_alternative<adc::Message>(prepared)) { fail(AdcCredentialErrorKind::crypto); }
    return adc::PasswordAuthentication{record.id.value(),std::get<adc::Message>(std::move(prepared)),record.role};
}
bool AdcAuthenticationDirectory::allows_identity(std::string_view nickname,std::string_view cid,std::optional<std::uint64_t> account) const noexcept {
    const auto nick = impl_->nicknames.find(nickname), client = impl_->cids.find(cid);
    if (!account) { return nick == impl_->nicknames.end() && client == impl_->cids.end(); }
    return nick != impl_->nicknames.end() && client != impl_->cids.end() && nick->second == client->second && impl_->records[nick->second].id.value() == *account;
}
} // namespace dc24h::accounts
