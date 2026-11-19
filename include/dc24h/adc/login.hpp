#pragma once

#include "dc24h/adc/inf.hpp"
#include "dc24h/admin/kick.hpp"
#include "dc24h/admin/mute.hpp"
#include "dc24h/bans/bans.hpp"
#include "dc24h/adc/authentication.hpp"
#include "dc24h/security/roles.hpp"
#include "dc24h/security/authentication_limits.hpp"
#include "dc24h/adc/user_login.hpp"
#include "dc24h/adc/user_list.hpp"
#include "dc24h/adc/inf_updates.hpp"
#include "dc24h/adc/user_disconnect.hpp"
#include "dc24h/users/session_manager.hpp"
#include "dc24h/chat/core.hpp"
#include "dc24h/adc/message_routing.hpp"
#include "dc24h/adc/hub_messages.hpp"
#include "dc24h/chat/anti_flood.hpp"
#include "dc24h/chat/statistics.hpp"
#include "dc24h/adc/search_routing.hpp"
#include "dc24h/adc/result_routing.hpp"
#include "dc24h/adc/connection_routing.hpp"
#include "dc24h/adc/status_routing.hpp"
#include "dc24h/transfers/statistics.hpp"
#include "dc24h/search/security.hpp"
#include "dc24h/search/statistics.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace dc24h::adc {
struct LoginLimits {
    std::size_t connections{1024};
    std::size_t output_per_client{256 * 1024};
    std::size_t output_total{16 * 1024 * 1024};
    users::Limits users{};
    // Transport policy: Network always supplies its validated chat limits.
    // Trusted in-process callers may omit them, as with IP/frame admission.
    std::optional<chat::FloodLimits> anti_flood{};
    chat::Policy chat{};
    std::optional<std::string> welcome_text{std::string(chat::default_welcome_message)};
    search::SecuritySettings search{};
    transfers::SecuritySettings transfers{};
    std::shared_ptr<const AuthenticationSource> authentication{};
    security::AuthenticationLimits authentication_limits{};
    security::RolePolicy roles{};
    std::shared_ptr<const bans::Directory> bans{};
};
enum class CloseReason { login_timeout, idle_timeout, partial_frame_timeout, security_quota, negotiation_limit, identity_limit };

// Single reactor-thread owner of guest admission, public presence and ordered
// output. IDs are transport tokens, never fd numbers. No socket/SQL/logger I/O.
// Chat, Search and Transfers delegate to their typed adapters and cores.
class LoginHub final : private transfers::PermissionSource {
public:
    using Id = std::uint64_t;
    explicit LoginHub(LoginLimits limits = {});
    ~LoginHub();
    LoginHub(const LoginHub&) = delete;
    LoginHub& operator=(const LoginHub&) = delete;
    void connect(Id id, std::string_view numeric_peer, users::Transport transport = users::Transport::adc,
                 users::TimePoint accepted = users::Clock::now());
    void transport_ready(Id id, users::TimePoint now = users::Clock::now());
    void receive(Id id, const Message& message, chat::FloodClock::time_point chat_now = chat::FloodClock::now());
    // Trusted owner-thread capability update for a ready lifetime ID only.
    // Client INF/SUP never grants permissions. Revoking queries cancels that
    // owner's token windows; rate credits are preserved across permission changes.
    [[nodiscard]] bool set_search_permissions(Id id, search::Permissions permissions) noexcept;
    // Trusted owner-thread grant, reset with this connection's Peer lifetime.
    // Client metadata cannot grant or override transfer permissions.
    [[nodiscard]] bool set_transfer_permissions(Id id, transfers::Permissions permissions) noexcept;
    [[nodiscard]] bool needs_identity_check(Id id, const Message& message) const;
    // Sanitized terminal notification; idempotent and bounded by existing output
    // caps. Assigned clients receive ISTA then a final own IQUI. No clocks here.
    void close(Id id, CloseReason reason, users::TimePoint now = users::Clock::now());
    // Borrowed immutable front until consume/disconnect, including across queue
    // appends. A TLS WANT retry must use the same address and remaining length.
    [[nodiscard]] std::string_view output(Id id) const;
    // Bytes actually accepted by transport. Commits state only at the final LF.
    // True only on the first completed welcome: caller releases pending admission
    // and emits readiness once. Partial/terminal/ordinary sends return false.
    bool consume(Id id, std::size_t bytes);
    // Process shutdown can suppress public QUIT broadcasts to other sockets.
    void disconnect(Id id, bool announce = true, users::CloseCause cause = users::CloseCause::peer_closed,
                    users::TimePoint now = users::Clock::now());
    // Borrowed model is valid only while this ID remains registered, on the
    // owner thread. Snapshot is an owning copy; stale/missing IDs return nullopt.
    [[nodiscard]] const users::Session& session_model(Id id) const;
    [[nodiscard]] std::optional<users::Snapshot> snapshot(Id id) const;
    [[nodiscard]] const users::SessionManager& sessions() const noexcept { return sessions_; }
    [[nodiscard]] search::Statistics search_statistics() const noexcept {
        auto value=search_statistics_;value.tracked_searches=search_.tracked_searches();return value;
    }
    [[nodiscard]] admin::MuteStatistics mute_statistics() const noexcept { return mute_statistics_; }
    [[nodiscard]] admin::KickStatistics kick_statistics() const noexcept { return kick_statistics_; }
    [[nodiscard]] chat::Statistics chat_statistics() const noexcept { return chat_statistics_; }
    [[nodiscard]] transfers::Statistics transfer_statistics() const noexcept { return transfer_statistics_; }
    [[nodiscard]] bool closing(Id id) const;
    [[nodiscard]] bool must_close(Id id) const;
    [[nodiscard]] bool overloaded(Id id) const;
    [[nodiscard]] bool peer_fatal(Id id) const;
    [[nodiscard]] bool ready(Id id) const;
    // Trusted owner-thread account lookup, published only after final welcome LF.
    [[nodiscard]] std::optional<std::uint64_t> authenticated_account(Id id) const;
    [[nodiscard]] std::optional<users::AuthSession> authenticated_session(Id id) const noexcept;
    // Trusted owner-thread operation. Immediately disables this ready lifetime,
    // closes its transport on the next reactor pass; does not revoke credentials
    // or prevent a fresh GPA/PAS login. No allocations, SQL or automatic grants.
    [[nodiscard]] bool revoke_auth_session(Id id, users::TimePoint now = users::Clock::now()) noexcept;
    // Role policy only, never bypasses global feature switches, flood budgets or
    // per-session overrides. Ready unauthenticated lifetimes use Guest;
    // missing/pending/closing lifetimes deny.
    [[nodiscard]] bool role_allows(Id id, security::Permission permission) const noexcept;
    // Trusted adapter supplies the ACTUAL transport actor, never a client-chosen
    // account/SID. The actor must be authenticated; the ready target may be Guest.
    [[nodiscard]] bool revoke_auth_session_as(Id actor, Id target,
        users::TimePoint now = users::Clock::now()) noexcept;
    // Live kick: hierarchy checked, target immediately loses routing/auth access,
    // bounded terminal notice drains before transport close. No ban or SQL write.
    [[nodiscard]] admin::KickError kick(Id actor,Id target,std::string_view reason={},
        users::TimePoint now=users::Clock::now());
    [[nodiscard]] admin::MuteError mute(Id actor,Id target,admin::MuteScope scope,bool enabled,
        users::TimePoint now=users::Clock::now());
    [[nodiscard]] std::optional<users::ChatMute> chat_mute(Id id) const noexcept;
    [[nodiscard]] ConnectionState state(Id id) const;
    [[nodiscard]] std::size_t queued_bytes() const noexcept { return queued_; }
    [[nodiscard]] std::size_t users() const noexcept { return sessions_.identified_users(); }
    // Trusted in-process API, never called with client-supplied text by receive.
    // No target means current ready users; a target is a lifetime ID, not a SID.
    // Preparation failures leave queues unchanged; per-recipient queue faults
    // close that recipient only. Allocation before dispatch propagates.
    [[nodiscard]] std::variant<chat::DispatchReport, HubMessageError> send_hub_message(
        std::string_view text, std::optional<users::SessionId> target = {});
private:
    [[nodiscard]] std::optional<transfers::Permissions> connection_permissions(users::SessionId id) const noexcept override;
    struct Peer;
    enum class Completion { observe, support, authentication, welcome };
    void complete_login(Peer& peer);
    void limit_authentication(Peer& peer);
    bool enqueue(Peer& peer, Message message, Completion completion = Completion::observe);
    bool enqueue_prepared(Peer& peer, const Message& message, std::string wire, Completion completion);
    template<class Packet> bool enqueue_shared(Peer& peer, std::shared_ptr<const Packet> frame);
    void reject(Peer& peer, std::string_view status, std::optional<std::string_view> command = {},
                std::string_view description = {}, users::CloseCause cause = users::CloseCause::protocol_rejected,
                users::TimePoint now = users::Clock::now(), std::optional<std::int64_t> reconnect_after = {});
    bool admit_search(Peer& peer, const Message& message, search::Operation operation, users::TimePoint now, search::Attempt& attempt);
    void receive_result(Id id, const Message& message, users::TimePoint now, search::Attempt& attempt);
    void receive_search(Id id, const Message& message, users::TimePoint now, search::Attempt& attempt);
    void receive_connection(Id id, const Message& message, users::TimePoint now, transfers::Attempt& attempt);
    void receive_impl(Id id, const Message& message, chat::FloodClock::time_point chat_now, transfers::Attempt& transfer_attempt);
    void route_status(Id id, const Message& message);
    bool route_mute(Id id,const Message& message,users::TimePoint now);
    bool route_kick(Id id,const Message& message,users::TimePoint now);
    void route_chat(Id id, const Message& message, chat::Attempt& attempt);
    void relay(Id except, const Message& message);
    void announce_departure(const Departure& departure);
    LoginLimits limits_;
    security::AuthenticationLimiter authentication_limiter_;
    std::shared_ptr<SidPool> pool_;
    users::SessionManager sessions_; // Outlives the peers that borrow models.
    admin::KickCore kick_core_;
    admin::MuteCore mute_core_;
    search::Core search_;
    SearchRouter search_router_;
    ResultRouter result_router_;
    transfers::Core transfers_;
    ConnectionRouter connection_router_;
    StatusRouter status_router_;
    chat::Core chat_;
    MessageRouter message_router_;
    HubMessages hub_messages_;
    SharedHubMessageFrame welcome_message_;
    UserLogin user_login_;
    UserList user_list_;
    InfUpdates inf_updates_;
    UserDisconnect user_disconnect_;
    std::unordered_map<Id, std::unique_ptr<Peer>> peers_;
    std::size_t queued_{0};
    admin::KickStatistics kick_statistics_{};
    admin::MuteStatistics mute_statistics_{};
    chat::Statistics chat_statistics_{};
    search::Statistics search_statistics_{};
    transfers::Statistics transfer_statistics_{};
};
} // namespace dc24h::adc
