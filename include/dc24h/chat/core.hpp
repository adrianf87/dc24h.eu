#pragma once

#include "dc24h/users/session_manager.hpp"

#include <functional>
#include <array>
#include <memory>
#include <span>
#include <utility>

namespace dc24h::chat {
namespace detail { struct RoutingDomain final {}; }
enum class Error { missing_sender, sender_not_ready, wrong_source, invalid_text, missing_sink, recipient_unavailable, foreign_plan };
// Owned immutable admission snapshot. Lifetime IDs, never fd numbers or SIDs,
// identify recipients; no nicknames, credentials or private metadata are copied.
class Broadcast final {
public:
    [[nodiscard]] users::SessionId sender() const noexcept { return sender_; }
    [[nodiscard]] const std::string& source() const noexcept { return source_; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] std::optional<bool> third_person() const noexcept { return third_person_; }
    [[nodiscard]] const std::vector<users::SessionId>& recipients() const noexcept { return recipients_; }
private:
    friend class Core;
    Broadcast(std::shared_ptr<const detail::RoutingDomain> domain, users::SessionId sender, std::string_view source, std::string_view text,
              std::optional<bool> third_person, std::vector<users::SessionId> recipients)
        : domain_(std::move(domain)), sender_(sender), source_(source), text_(text), third_person_(third_person), recipients_(std::move(recipients)) {}
    std::shared_ptr<const detail::RoutingDomain> domain_;
    users::SessionId sender_;
    std::string source_, text_;
    std::optional<bool> third_person_;
    std::vector<users::SessionId> recipients_;
};
// Directed delivery binds the target lifetime as well as its protocol key.
// Echo selects target then sender, collapsing self-directed messages to one ID.
class Directed final {
public:
    [[nodiscard]] users::SessionId sender() const noexcept { return sender_; }
    [[nodiscard]] users::SessionId target() const noexcept { return recipients_[0]; }
    [[nodiscard]] const std::string& source() const noexcept { return source_; }
    [[nodiscard]] const std::string& target_key() const noexcept { return target_key_; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] std::optional<bool> third_person() const noexcept { return third_person_; }
    [[nodiscard]] std::span<const users::SessionId> recipients() const noexcept { return {recipients_.data(),count_}; }
private:
    friend class Core;
    Directed(std::shared_ptr<const detail::RoutingDomain> domain, users::SessionId sender, users::SessionId target, std::string_view source,
             std::string_view target_key, std::string_view text, std::optional<bool> third_person, bool echo)
        : domain_(std::move(domain)), sender_(sender), source_(source), target_key_(target_key), text_(text), third_person_(third_person),
          recipients_{target,sender}, count_(echo && target!=sender ? 2U : 1U) {}
    std::shared_ptr<const detail::RoutingDomain> domain_;
    users::SessionId sender_;
    std::string source_,target_key_,text_;
    std::optional<bool> third_person_;
    std::array<users::SessionId,2> recipients_;
    std::size_t count_;
};
struct DispatchReport {
    std::size_t queued{}, unavailable{}, refused{};
    friend bool operator==(const DispatchReport&, const DispatchReport&) = default;
};
// Single owner/thread, no sockets, ADC, SQL, logger, history or persistent queues.
// Supports public broadcasts and directed/echo delivery. Protocol conversation
// policy and transport exceptions remain adapter responsibilities.
class Core final {
public:
    using Sink = std::function<bool(users::SessionId, const Broadcast&)>;
    using DirectedSink = std::function<bool(users::SessionId, const Directed&)>;
    explicit Core(const users::SessionManager& sessions) : sessions_(sessions) {}
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    [[nodiscard]] std::optional<Error> validate_sender(users::SessionId sender, std::string_view source) const noexcept;
    [[nodiscard]] std::variant<Broadcast, Error> prepare(users::SessionId sender,
        std::string_view source, std::string_view text, std::optional<bool> third_person = {}) const;
    // Rechecks the sender and each recipient lifetime before calling the sink.
    // False means queue refusal, not transport delivery. Trusted sinks may close
    // sessions but must not throw; an exception propagates after possible partial
    // delivery, so callers must never blindly retry a plan.
    [[nodiscard]] std::variant<DispatchReport, Error> dispatch(const Broadcast& message, const Sink& sink) const;
    [[nodiscard]] std::variant<Directed, Error> prepare_direct(users::SessionId sender,
        std::string_view source, std::string_view target, std::string_view text,
        std::optional<bool> third_person = {}, bool echo = false) const;
    [[nodiscard]] std::variant<DispatchReport, Error> dispatch(const Directed& message, const DirectedSink& sink) const;
private:
    const users::SessionManager& sessions_;
    // A retained token prevents cross-engine dispatch, even after an old Core
    // address is reused. Plans do not extend the registry or engine lifetime.
    const std::shared_ptr<const detail::RoutingDomain> domain_{std::make_shared<const detail::RoutingDomain>()};
};
} // namespace dc24h::chat
