#pragma once
#include "dc24h/accounts/account.hpp"
#include "dc24h/adc/authentication.hpp"
#include "dc24h/database/database.hpp"
#include <memory>
#include <stdexcept>

namespace dc24h::accounts {
enum class AdcCredentialErrorKind { input, proof, conflict, mapping, schema, invalid_record, crypto, same_password };
class AdcCredentialError final : public std::runtime_error {
public:
    explicit AdcCredentialError(AdcCredentialErrorKind kind);
    [[nodiscard]] AdcCredentialErrorKind kind() const noexcept { return kind_; }
private:
    AdcCredentialErrorKind kind_;
};
// External 32-byte AES key, encoded as 64 lowercase hex characters in a private
// effective-UID-owned file. Never exported, copied, logged or stored in SQL.
class AdcSecretKey final {
public:
    [[nodiscard]] static std::shared_ptr<const AdcSecretKey> load(const std::string& path);
    ~AdcSecretKey();
    AdcSecretKey(const AdcSecretKey&) = delete;
    AdcSecretKey& operator=(const AdcSecretKey&) = delete;
private:
    AdcSecretKey() = default;
    friend class AdcCredentialService;
    friend class AdcAuthenticationDirectory;
    std::array<unsigned char,32> bytes_{};
};
// Trusted, synchronous administrator API. The caller authorizes enrollment;
// proof of the existing scrypt password is additionally required. The ADC
// password must differ byte for byte from that account password. No
// replacement/reset or public endpoint.
class AdcCredentialService final {
public:
    AdcCredentialService(database::Connection& connection,const AdcSecretKey& key) noexcept : connection_(connection),key_(key) {}
    void enroll(AccountId account,std::string_view account_password,std::string_view adc_password,Timestamp created_at);
private:
    database::Connection& connection_;
    const AdcSecretKey& key_;
};
// Bounded immutable startup snapshot of account policy, identity mappings and
// ciphertext. Reads SQL only in load(). prepare does not decrypt. Plaintext is
// decrypted and wiped only while opening one admitted challenge.
// Changes/revocations require a hub restart in this stage.
class AdcAuthenticationDirectory final : public adc::AuthenticationSource {
public:
    [[nodiscard]] static std::shared_ptr<const AdcAuthenticationDirectory> load(database::Connection& connection,
        std::shared_ptr<const AdcSecretKey> key,std::size_t max_accounts = 1024,bool require_tls = true);
    ~AdcAuthenticationDirectory() override;
    [[nodiscard]] adc::AuthenticationPreparation prepare(std::string_view nickname,std::string_view cid,
        users::Transport transport,adc::PasswordChallenge& challenge) const override;
    [[nodiscard]] std::optional<adc::Message> open_challenge(std::uint64_t account,adc::PasswordChallenge& challenge) const override;
    [[nodiscard]] bool allows_identity(std::string_view nickname,std::string_view cid,
        std::optional<std::uint64_t> account) const noexcept override;
private:
    struct Impl;
    explicit AdcAuthenticationDirectory(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};
} // namespace dc24h::accounts
