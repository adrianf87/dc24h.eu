#pragma once

#include "dc24h/users/session_manager.hpp"

#include <functional>
#include <utility>

namespace dc24h::chat {
enum class Error { missing_sender, sender_not_ready, wrong_source, invalid_text, missing_sink };
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
    Broadcast(users::SessionId sender, std::string_view source, std::string_view text,
              std::optional<bool> third_person, std::vector<users::SessionId> recipients)
        : sender_(sender), source_(source), text_(text), third_person_(third_person), recipients_(std::move(recipients)) {}
    users::SessionId sender_;
    std::string source_, text_;
    std::optional<bool> third_person_;
    std::vector<users::SessionId> recipients_;
};
struct DispatchReport {
    std::size_t queued{}, unavailable{}, refused{};
    friend bool operator==(const DispatchReport&, const DispatchReport&) = default;
};
// Single owner/thread, no sockets, ADC, SQL, logger, history or persistent queues.
// This first handler supports public broadcasts. Other chat modes are adapter
// refusals until their delivery/conversation policy is implemented.
class Core final {
public:
    using Sink = std::function<bool(users::SessionId, const Broadcast&)>;
    explicit Core(const users::SessionManager& sessions) noexcept : sessions_(sessions) {}
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    [[nodiscard]] std::variant<Broadcast, Error> prepare(users::SessionId sender,
        std::string_view source, std::string_view text, std::optional<bool> third_person = {}) const;
    // Rechecks the sender and each recipient lifetime before calling the sink.
    // False means queue refusal, not transport delivery. Trusted sinks may close
    // sessions but must not throw; an exception propagates after possible partial
    // delivery, so callers must never blindly retry a plan.
    [[nodiscard]] std::variant<DispatchReport, Error> dispatch(const Broadcast& message, const Sink& sink) const;
private:
    [[nodiscard]] std::optional<Error> check_sender(users::SessionId sender, std::string_view source) const noexcept;
    const users::SessionManager& sessions_;
};
} // namespace dc24h::chat
