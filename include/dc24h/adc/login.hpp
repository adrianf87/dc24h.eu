#pragma once

#include "dc24h/adc/inf.hpp"
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
};
enum class CloseReason { login_timeout, idle_timeout, partial_frame_timeout, security_quota, negotiation_limit, identity_limit };

// Single reactor-thread owner of guest admission, public presence and ordered
// output. IDs are transport tokens, never fd numbers. No socket/SQL/logger I/O.
// Chat delegates to typed adapters and Chat Core. Accounts/search/transfers remain separate.
class LoginHub final {
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
    [[nodiscard]] chat::Statistics chat_statistics() const noexcept { return chat_statistics_; }
    [[nodiscard]] bool closing(Id id) const;
    [[nodiscard]] bool must_close(Id id) const;
    [[nodiscard]] bool overloaded(Id id) const;
    [[nodiscard]] bool peer_fatal(Id id) const;
    [[nodiscard]] bool ready(Id id) const;
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
    struct Peer;
    enum class Completion { observe, support, welcome };
    bool enqueue(Peer& peer, Message message, Completion completion = Completion::observe);
    template<class Packet> bool enqueue_shared(Peer& peer, std::shared_ptr<const Packet> frame);
    void reject(Peer& peer, std::string_view status, std::optional<std::string_view> command = {},
                std::string_view description = {}, users::CloseCause cause = users::CloseCause::protocol_rejected,
                users::TimePoint now = users::Clock::now());
    void route_status(Id id, const Message& message);
    void route_chat(Id id, const Message& message, chat::Attempt& attempt);
    void relay(Id except, const Message& message);
    void announce_departure(const Departure& departure);
    LoginLimits limits_;
    std::shared_ptr<SidPool> pool_;
    users::SessionManager sessions_; // Outlives the peers that borrow models.
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
    chat::Statistics chat_statistics_{};
};
} // namespace dc24h::adc
