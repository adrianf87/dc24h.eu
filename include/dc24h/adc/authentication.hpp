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
struct PasswordAuthentication { std::uint64_t account_id; Message challenge; };
using AuthenticationPreparation = std::variant<GuestAuthentication,DeniedAuthentication,PasswordAuthentication>;
// Immutable trusted startup policy: no SQL, blocking work or password KDF on
// the reactor. A bounded secret may be decrypted to prepare a Tiger challenge.
class AuthenticationSource {
public:
    virtual ~AuthenticationSource() = default;
    [[nodiscard]] virtual AuthenticationPreparation prepare(std::string_view nickname, std::string_view cid,
        users::Transport transport, PasswordChallenge& challenge) const = 0;
    [[nodiscard]] virtual bool allows_identity(std::string_view nickname, std::string_view cid,
        std::optional<std::uint64_t> account) const noexcept = 0;
};
} // namespace dc24h::adc
