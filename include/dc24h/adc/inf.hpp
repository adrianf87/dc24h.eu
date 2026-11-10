#pragma once

#include "dc24h/adc/sid_session.hpp"
#include "dc24h/adc/tiger_identity.hpp"

#include <array>
#include <cstddef>
#include <map>

namespace dc24h::adc {
inline constexpr std::size_t max_info_bytes = 4096; // Sum of retained field names/values.
inline constexpr std::size_t max_nick_bytes = 64;
enum class InfError {
    invalid_message, wrong_source, duplicate_field, missing_identity, invalid_cid,
    invalid_pid, cid_mismatch, invalid_nick, invalid_text, invalid_integer,
    invalid_ip, invalid_features, privilege_claim, field_limit,
    immutable_identity, unexpected_pid, unsupported_hash, crypto_unavailable
};
[[nodiscard]] std::string_view inf_status_code(InfError error) noexcept;

// Validated public metadata only. Never contains PD or unknown extension fields.
class ClientInfo final {
public:
    ClientInfo(const ClientInfo&) = default;
    ClientInfo(ClientInfo&&) noexcept = default;
    ClientInfo& operator=(const ClientInfo&) = default;
    ClientInfo& operator=(ClientInfo&&) noexcept = default;
    ~ClientInfo() = default;
    [[nodiscard]] std::optional<std::string_view> field(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::int64_t> integer(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<VerifiedIdentity> identity() const noexcept { return identity_; }
    [[nodiscard]] Message public_message(Sid source) const;
    [[nodiscard]] std::size_t bytes() const noexcept;
    friend bool operator==(const ClientInfo&, const ClientInfo&) = default;
private:
    friend class InfValidator;
    ClientInfo() = default;
    std::map<std::string, std::string, std::less<>> fields_;
    std::optional<VerifiedIdentity> identity_;
};

// Trusted kernel peer address is numeric, without port/zone; constructor rejects
// invalid addresses. IPv4-mapped IPv6 peers are normalized to IPv4.
class InfValidator final {
public:
    explicit InfValidator(std::string_view peer_address);
    // Parser-produced, decoded BINF only. First snapshot requires ID/PD/NI;
    // later updates forbid PD and keep ID immutable. No mutation on failure.
    [[nodiscard]] std::variant<ClientInfo, InfError> validate(const Message& message, Sid source,
        SessionHash hash, const ClientInfo* previous = nullptr) const;
private:
    int family_;
    std::array<unsigned char, 16> address_{};
};

using ClientSessionError = std::variant<StateError, SupportError, SidError, InfError>;
using ClientSessionResult = std::optional<ClientSessionError>;
// INF/SID/SUP/state composition. Registry uniqueness/admission and passwords
// remain external decisions; no sockets, dispatch, SQL or logger dependencies.
class ClientSession final {
public:
    ClientSession(std::shared_ptr<SidPool> pool, std::string_view peer_address, SupportPolicy policy = {});
    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;
    ClientSession(ClientSession&&) = delete;
    ClientSession& operator=(ClientSession&&) = delete;
    [[nodiscard]] ClientSessionResult observe(Direction direction, const Message& message);
    [[nodiscard]] ClientSessionResult approve_identity(IdentityMode mode) noexcept;
    [[nodiscard]] StateResult approve_password() noexcept { return session_.approve_password(); }
    [[nodiscard]] std::optional<SidError> prepare_sid() noexcept { return session_.prepare_sid(); }
    [[nodiscard]] std::optional<Message> sid_reply() const { return session_.sid_reply(); }
    [[nodiscard]] std::optional<std::string> sid_reply_line() const { return session_.sid_reply_line(); }
    [[nodiscard]] ConnectionState state() const noexcept { return session_.state(); }
    [[nodiscard]] std::string_view sid() const noexcept { return session_.sid(); }
    [[nodiscard]] const SupportNegotiation& support() const noexcept { return session_.support(); }
    [[nodiscard]] const std::optional<ClientInfo>& info() const noexcept { return info_; }
    [[nodiscard]] StateResult keepalive() const noexcept { return session_.keepalive(); }
    void disconnect() noexcept;
private:
    friend class InfUpdates;
    // Ready-state staging for the transactional live INF updater. These do not
    // expose mutable metadata or bypass the validator's public-field policy.
    [[nodiscard]] std::variant<ClientInfo, ClientSessionError> prepare_update(const Message& message) const;
    void commit_update(ClientInfo info) noexcept;
    SidSession session_;
    InfValidator validator_;
    std::optional<ClientInfo> info_;
};
} // namespace dc24h::adc
