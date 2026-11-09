#pragma once

#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/sup.hpp"

namespace dc24h::adc {

// Canonical public CID shape. Parsing alone does not verify its PID relationship.
class TigerCid final {
public:
    [[nodiscard]] static std::optional<TigerCid> parse(std::string_view text) noexcept;
    [[nodiscard]] std::string text() const;
    [[nodiscard]] const TigerDigest& bytes() const noexcept { return bytes_; }
    friend bool operator==(const TigerCid&, const TigerCid&) = default;
private:
    explicit TigerCid(const TigerDigest& bytes) noexcept : bytes_(bytes) {}
    TigerDigest bytes_;
};

enum class IdentityError { invalid_cid, invalid_pid, mismatch, unsupported_hash, crypto_unavailable };
[[nodiscard]] std::string_view identity_status_code(IdentityError error) noexcept;

// Public result of a successful Tiger(PID)==CID check. It contains only the CID,
// never PID bytes/text. It is not an account credential or admission decision.
class VerifiedIdentity final {
public:
    VerifiedIdentity(const VerifiedIdentity&) = default;
    VerifiedIdentity(VerifiedIdentity&&) noexcept = default;
    VerifiedIdentity& operator=(const VerifiedIdentity&) = default;
    VerifiedIdentity& operator=(VerifiedIdentity&&) noexcept = default;
    [[nodiscard]] const TigerCid& cid() const noexcept { return cid_; }
    friend bool operator==(const VerifiedIdentity&, const VerifiedIdentity&) = default;
private:
    friend std::variant<VerifiedIdentity, IdentityError> verify_tiger_identity(
        std::string_view, std::string_view, SessionHash) noexcept;
    explicit VerifiedIdentity(TigerCid cid) noexcept : cid_(cid) {}
    TigerCid cid_;
};

// Borrows encoded input. Private decoded/derived buffers are noncopyable locals
// and cleansed on every exit. Final 24-byte comparison uses CRYPTO_memcmp;
// syntax validation and provider operations are not claimed constant time.
[[nodiscard]] std::variant<VerifiedIdentity, IdentityError> verify_tiger_identity(
    std::string_view cid, std::string_view pid, SessionHash hash = SessionHash::tiger192) noexcept;

} // namespace dc24h::adc
