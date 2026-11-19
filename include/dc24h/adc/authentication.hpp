#pragma once
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/parser.hpp"
#include "dc24h/adc/sup.hpp"
#include "dc24h/users/session.hpp"

namespace dc24h::adc {
enum class AuthenticationError { invalid_password, unsupported_hash, crypto, sequence, invalid_response, mismatch };
// One challenge per connection lifetime. No copying, moving, reset or replay.
class PasswordChallenge final {
public:
    PasswordChallenge() = default;
    ~PasswordChallenge();
    PasswordChallenge(const PasswordChallenge&) = delete;
    PasswordChallenge& operator=(const PasswordChallenge&) = delete;
    [[nodiscard]] std::variant<Message,AuthenticationError> issue(std::string_view password, SessionHash hash);
    // Only after the complete IGPA, including LF, was accepted by transport.
    [[nodiscard]] bool sent() noexcept;
    [[nodiscard]] std::optional<AuthenticationError> verify(const Message& response) noexcept;
    void cancel() noexcept;
    [[nodiscard]] bool pending() const noexcept { return phase_ == Phase::prepared; }
private:
    enum class Phase { fresh, prepared, sent, spent };
    Phase phase_{Phase::fresh};
    TigerDigest expected_{};
};
struct GuestAuthentication {};
struct DeniedAuthentication {};
struct PasswordAuthentication {
    std::uint64_t account_id;
    Message challenge;
    accounts::Role role{accounts::Role::registered};
    // prepare identified the account and left its ADC secret sealed.
    bool challenge_deferred{false};
};
using AuthenticationPreparation = std::variant<GuestAuthentication,DeniedAuthentication,PasswordAuthentication>;
// Immutable trusted startup policy: no SQL, blocking work or password KDF on
// the reactor. prepare may identify an account without decrypting. A bounded
// secret is decrypted only in open_challenge, after admission has been granted.
class AuthenticationSource {
public:
    virtual ~AuthenticationSource() = default;
    [[nodiscard]] virtual AuthenticationPreparation prepare(std::string_view nickname, std::string_view cid,
        users::Transport transport, PasswordChallenge& challenge) const = 0;
    // Default: prepare already issued the challenge. A deferred directory
    // decrypts here and returns the IGPA message. Null means it cannot.
    [[nodiscard]] virtual std::optional<Message> open_challenge(std::uint64_t, PasswordChallenge&) const {
        return std::nullopt;
    }
    [[nodiscard]] virtual bool allows_identity(std::string_view nickname, std::string_view cid,
        std::optional<std::uint64_t> account) const noexcept = 0;
};
} // namespace dc24h::adc
