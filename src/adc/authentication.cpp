#include "dc24h/adc/authentication.hpp"
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <utf8proc.h>
#include <algorithm>

namespace dc24h::adc {
namespace {
template<std::size_t N> struct PrivateBytes {
    std::array<std::uint8_t,N> bytes{};
    PrivateBytes() = default;
    PrivateBytes(const PrivateBytes&) = delete;
    ~PrivateBytes() { OPENSSL_cleanse(bytes.data(),bytes.size()); }
};
bool valid_password(std::string_view password) noexcept {
    if (password.empty() || password.size() > 1024) { return false; }
    std::size_t offset = 0;
    while (offset < password.size()) {
        utf8proc_int32_t character{};
        const auto size = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(password.data()+offset),
            static_cast<utf8proc_ssize_t>(password.size()-offset),&character);
        if (size <= 0 || character < 32 || character == 127) { return false; }
        offset += static_cast<std::size_t>(size);
    }
    return true;
}
}
PasswordChallenge::~PasswordChallenge() { cancel(); }
void PasswordChallenge::cancel() noexcept {
    OPENSSL_cleanse(expected_.data(),expected_.size());
    phase_ = Phase::spent;
}
std::variant<Message,AuthenticationError> PasswordChallenge::issue(std::string_view password, SessionHash hash) {
    if (phase_ != Phase::fresh) { return AuthenticationError::sequence; }
    phase_ = Phase::spent;
    if (hash != SessionHash::tiger192) { return AuthenticationError::unsupported_hash; }
    if (!valid_password(password)) { return AuthenticationError::invalid_password; }
    PrivateBytes<24> salt;
    PrivateBytes<1024+24> input;
    PrivateBytes<24> expected;
    if (RAND_bytes(salt.bytes.data(),static_cast<int>(salt.bytes.size())) != 1) { return AuthenticationError::crypto; }
    std::copy(password.begin(),password.end(),input.bytes.begin());
    std::copy(salt.bytes.begin(),salt.bytes.end(),input.bytes.begin()+static_cast<std::ptrdiff_t>(password.size()));
    if (!tiger192_into(std::span(input.bytes).first(password.size()+salt.bytes.size()),expected.bytes)) {
        return AuthenticationError::crypto;
    }
    Message result{MessageType::info,"GPA",Command::gpa,{},{},{},{tiger_base32(salt.bytes)},1};
    std::copy(expected.bytes.begin(),expected.bytes.end(),expected_.begin());
    phase_ = Phase::prepared;
    return result;
}
bool PasswordChallenge::sent() noexcept {
    if (phase_ != Phase::prepared) { return false; }
    phase_ = Phase::sent;
    return true;
}
std::optional<AuthenticationError> PasswordChallenge::verify(const Message& response) noexcept {
    struct Consume { PasswordChallenge& self; ~Consume() { self.cancel(); } } consume{*this};
    if (phase_ != Phase::sent) { return AuthenticationError::sequence; }
    if (response.kind != Command::pas || response.command != "PAS" || response.type != MessageType::hub ||
        response.source || response.target || !response.features.empty() || response.parameters.size() != 1 ||
        response.positional_count != 1) { return AuthenticationError::invalid_response; }
    PrivateBytes<24> actual;
    if (!decode_tiger_base32(response.parameters[0],actual.bytes)) { return AuthenticationError::invalid_response; }
    if (CRYPTO_memcmp(expected_.data(),actual.bytes.data(),expected_.size()) != 0) { return AuthenticationError::mismatch; }
    return {};
}
} // namespace dc24h::adc
