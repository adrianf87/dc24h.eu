#include "dc24h/adc/tiger_identity.hpp"

#include <openssl/crypto.h>

namespace dc24h::adc {
namespace {
class PrivateDigest final {
public:
    PrivateDigest() = default;
    ~PrivateDigest() { OPENSSL_cleanse(bytes.data(), bytes.size()); }
    PrivateDigest(const PrivateDigest&) = delete;
    PrivateDigest& operator=(const PrivateDigest&) = delete;
    PrivateDigest(PrivateDigest&&) = delete;
    PrivateDigest& operator=(PrivateDigest&&) = delete;
    TigerDigest bytes{};
};
} // namespace

std::optional<TigerCid> TigerCid::parse(std::string_view text) noexcept {
    TigerDigest bytes{};
    if (!decode_tiger_base32(text, bytes)) { return std::nullopt; }
    return TigerCid(bytes);
}
std::string TigerCid::text() const { return tiger_base32(bytes_); }

std::string_view identity_status_code(IdentityError error) noexcept {
    switch (error) {
    case IdentityError::invalid_cid: return "243";
    case IdentityError::invalid_pid: case IdentityError::mismatch: return "227";
    case IdentityError::unsupported_hash: return "247";
    case IdentityError::crypto_unavailable: return "210";
    }
    return "240";
}

std::variant<VerifiedIdentity, IdentityError> verify_tiger_identity(
    std::string_view cid, std::string_view pid, SessionHash hash) noexcept {
    if (hash != SessionHash::tiger192) { return IdentityError::unsupported_hash; }
    const auto public_cid = TigerCid::parse(cid);
    if (!public_cid) { return IdentityError::invalid_cid; }
    PrivateDigest decoded_pid;
    if (!decode_tiger_base32(pid, decoded_pid.bytes)) { return IdentityError::invalid_pid; }
    PrivateDigest calculated;
    if (!tiger192_into(decoded_pid.bytes, calculated.bytes)) { return IdentityError::crypto_unavailable; }
    if (CRYPTO_memcmp(public_cid->bytes().data(), calculated.bytes.data(), calculated.bytes.size()) != 0) {
        return IdentityError::mismatch;
    }
    return VerifiedIdentity(*public_cid);
}
} // namespace dc24h::adc
