#pragma once

#include "dc24h/accounts/account.hpp"
#include "dc24h/adc/tiger_identity.hpp"
#include "dc24h/database/database.hpp"

#include <stdexcept>

namespace dc24h::accounts {
inline constexpr std::size_t max_identity_nickname_bytes = 64;
// An owning ADC nickname/CID pair, never proof of account authentication.
// The nickname is already-NFC UTF-8; the CID uses the canonical Tiger encoding.
class IdentityKey final {
public:
    IdentityKey(const IdentityKey&) = default;
    IdentityKey(IdentityKey&& other) noexcept;
    IdentityKey& operator=(IdentityKey other) noexcept;
    [[nodiscard]] static std::optional<IdentityKey> create(std::string_view nickname, std::string_view cid);
    [[nodiscard]] const std::string& nickname() const noexcept { return nickname_; }
    [[nodiscard]] const adc::TigerCid& cid() const noexcept { return cid_; }
    [[nodiscard]] bool valid() const noexcept { return !nickname_.empty() && nickname_.size() <= max_identity_nickname_bytes; }
    friend bool operator==(const IdentityKey&, const IdentityKey&) = default;
private:
    IdentityKey(std::string_view nickname, adc::TigerCid cid) : nickname_(nickname), cid_(cid) {}
    std::string nickname_;
    adc::TigerCid cid_;
};

struct StoredIdentity {
    AccountId account_id;
    IdentityKey identity;
    Timestamp created_at;
    friend bool operator==(const StoredIdentity&, const StoredIdentity&) = default;
};
enum class MappingErrorKind { invalid_identity, invalid_time, account_missing, conflict, invalid_record, schema };
class MappingError final : public std::runtime_error {
public:
    explicit MappingError(MappingErrorKind kind);
    [[nodiscard]] MappingErrorKind kind() const noexcept { return kind_; }
private:
    MappingErrorKind kind_;
};

// Trusted synchronous management API: authorization belongs to the caller.
// One immutable nickname/CID pair per account, with globally unique nickname
// and CID. No implicit replacement, registration, login or permission grant.
// Owns write transactions; borrows a connection on its owning thread. Requires
// explicit current migrations. Database unknown outcomes must be reconciled,
// never retried automatically. Returned values own their memory.
class IdentityRepository final {
public:
    explicit IdentityRepository(database::Connection& connection) noexcept : connection_(connection) {}
    [[nodiscard]] StoredIdentity bind(AccountId account, const IdentityKey& identity, Timestamp created_at);
    [[nodiscard]] std::optional<StoredIdentity> find_by_account(AccountId account);
    [[nodiscard]] std::optional<StoredIdentity> find_by_nickname(std::string_view nickname);
    [[nodiscard]] std::optional<StoredIdentity> find_by_cid(const adc::TigerCid& cid);
    // Both values must match the same row. Never falls back to either key.
    [[nodiscard]] std::optional<StoredIdentity> resolve(const IdentityKey& identity);
private:
    void require_schema();
    database::Connection& connection_;
};
} // namespace dc24h::accounts
