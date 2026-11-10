#pragma once

#include "dc24h/adc/state.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace dc24h::adc {

inline constexpr std::size_t max_support_features = 128; // Local per-peer/policy bound.
enum class SessionHash { tiger192 };
[[nodiscard]] constexpr std::string_view hash_feature(SessionHash) noexcept { return "TIGR"; }
[[nodiscard]] constexpr std::size_t hash_bytes(SessionHash) noexcept { return 24; }

enum class SupportError {
    invalid_message, feature_limit, missing_base, no_common_hash,
    session_hash_removed, invalid_sequence, reply_mismatch, local_update_unsupported
};
// Fatal status mapping used by login admission; never includes peer input.
[[nodiscard]] std::string_view support_status_code(SupportError error) noexcept;

struct SupportPolicy {
    // Caller must implement any additional local features it advertises.
    // TIGR is the only session hash currently recognized by this module.
    std::vector<std::string> features{"BASE", "TIGR"};
};

// Owned support metadata; no hash computation, network, identity or secrets.
// Wire-order AD/RM updates are transactional; errors leave the object unchanged.
class SupportNegotiation final {
public:
    // Invalid local policy throws invalid_argument before any peer is processed.
    explicit SupportNegotiation(SupportPolicy policy = {});
    [[nodiscard]] std::optional<SupportError> accept_initial(const Message& sup);
    [[nodiscard]] std::optional<SupportError> update_peer(const Message& sup);
    [[nodiscard]] std::optional<SessionHash> selected_hash() const noexcept { return hash_; }
    [[nodiscard]] bool peer_supports(std::string_view feature) const noexcept;
    [[nodiscard]] const std::vector<std::string>& peer_features() const noexcept { return peer_; }
    [[nodiscard]] const std::vector<std::string>& local_features() const noexcept { return local_; }
    // Owning, bounded initial ISUP; nullopt before successful negotiation.
    [[nodiscard]] std::optional<Message> initial_reply() const;
    // SUP-only wire encoding, including LF. Not a general ADC serializer.
    [[nodiscard]] std::optional<std::string> initial_reply_line() const;

private:
    std::vector<std::string> local_; // Canonical sorted set, duplicates collapsed.
    std::vector<std::string> peer_;
    std::optional<SessionHash> hash_;
};

using ProtocolError = std::variant<StateError, SupportError>;
using ProtocolResult = std::optional<ProtocolError>; // nullopt = accepted

// Composition of sequencing and negotiated support. No bypassing SUP approval
// is exposed. One noncopyable/nonmovable instance per connection/thread.
// Network attaches it through ClientSession/LoginHub and ordered send completions.
class ProtocolConnection final {
public:
    explicit ProtocolConnection(SupportPolicy policy = {});
    ProtocolConnection(const ProtocolConnection&) = delete;
    ProtocolConnection& operator=(const ProtocolConnection&) = delete;
    ProtocolConnection(ProtocolConnection&&) = delete;
    ProtocolConnection& operator=(ProtocolConnection&&) = delete;

    [[nodiscard]] ProtocolResult observe(Direction direction, const Message& message);
    [[nodiscard]] ConnectionState state() const noexcept { return state_.state(); }
    [[nodiscard]] std::string_view sid() const noexcept { return state_.sid(); }
    [[nodiscard]] const SupportNegotiation& support() const noexcept { return support_; }
    [[nodiscard]] StateResult keepalive() const noexcept { return state_.keepalive(); }
    // Semantic identity/password validation remains the caller's responsibility.
    [[nodiscard]] StateResult approve_identity(IdentityMode mode) noexcept { return state_.approve_identity(mode); }
    [[nodiscard]] StateResult approve_password() noexcept { return state_.approve_password(); }
    void disconnect() noexcept { state_.disconnect(); }

private:
    ConnectionStateMachine state_;
    SupportNegotiation support_;
};

} // namespace dc24h::adc
