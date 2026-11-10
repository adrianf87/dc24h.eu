#pragma once

#include "dc24h/adc/inf.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace dc24h::adc {
struct LoginLimits {
    std::size_t connections{1024};
    std::size_t output_per_client{256 * 1024};
    std::size_t output_total{16 * 1024 * 1024};
};
enum class CloseReason { login_timeout, idle_timeout, partial_frame_timeout, security_quota, negotiation_limit, identity_limit };

// Single reactor-thread owner of guest admission, public presence and ordered
// output. IDs are transport tokens, never fd numbers. No socket/SQL/logger I/O.
// Registered accounts/passwords and chat/search/transfer dispatch are not here.
class LoginHub final {
public:
    using Id = std::uint64_t;
    explicit LoginHub(LoginLimits limits = {});
    ~LoginHub();
    LoginHub(const LoginHub&) = delete;
    LoginHub& operator=(const LoginHub&) = delete;
    void connect(Id id, std::string_view numeric_peer);
    void receive(Id id, const Message& message);
    [[nodiscard]] bool needs_identity_check(Id id, const Message& message) const;
    // Sanitized terminal notification; idempotent and bounded by existing output
    // caps. Assigned clients receive ISTA then a final own IQUI. No clocks here.
    void close(Id id, CloseReason reason);
    // Borrowed immutable front until consume/disconnect, including across queue
    // appends. A TLS WANT retry must use the same address and remaining length.
    [[nodiscard]] std::string_view output(Id id) const;
    // Bytes actually accepted by transport. Commits state only at the final LF.
    void consume(Id id, std::size_t bytes);
    // Process shutdown can suppress public QUIT broadcasts to other sockets.
    void disconnect(Id id, bool announce = true);
    [[nodiscard]] bool closing(Id id) const;
    [[nodiscard]] bool must_close(Id id) const;
    [[nodiscard]] bool overloaded(Id id) const;
    [[nodiscard]] bool peer_fatal(Id id) const;
    [[nodiscard]] bool ready(Id id) const;
    [[nodiscard]] ConnectionState state(Id id) const;
    [[nodiscard]] std::size_t queued_bytes() const noexcept { return queued_; }
    [[nodiscard]] std::size_t users() const noexcept { return nicknames_.size(); }
private:
    struct Peer;
    enum class Completion { observe, support, welcome };
    bool enqueue(Peer& peer, Message message, Completion completion = Completion::observe);
    void reject(Peer& peer, std::string_view status, std::optional<std::string_view> command = {},
                std::string_view description = {});
    void route_status(Id id, const Message& message);
    void relay(Id except, const Message& message);
    void remove_identity(Id id, const Peer& peer);
    LoginLimits limits_;
    std::shared_ptr<SidPool> pool_;
    std::unordered_map<Id, std::unique_ptr<Peer>> peers_;
    std::unordered_map<std::string, Id> nicknames_;
    std::unordered_map<std::string, Id> cids_;
    std::unordered_map<std::string, Id> sids_;
    std::size_t queued_{0};
};
} // namespace dc24h::adc
