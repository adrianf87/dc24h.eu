#pragma once
#include "dc24h/adc/status.hpp"
#include "dc24h/users/session_manager.hpp"
#include <functional>
#include <span>

namespace dc24h::adc {
enum class StatusRoutingError { invalid_state, wrong_source, invalid_message, recipient_unavailable,
    foreign_plan, missing_sink, already_dispatched };
namespace detail { struct StatusDomain final {}; }
class StatusFrame final {
public:
    class Passkey { friend class StatusRouter; Passkey() noexcept = default; };
    StatusFrame(const StatusFrame&) = delete;
    StatusFrame& operator=(const StatusFrame&) = delete;
    StatusFrame(StatusFrame&&) = delete;
    StatusFrame& operator=(StatusFrame&&) = delete;
    StatusFrame(Passkey, Message message, std::string wire, StatusCode code)
        : message_(std::move(message)), wire_(std::move(wire)), code_(code) {}
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
    [[nodiscard]] StatusCode code() const noexcept { return code_; }
private:
    Message message_;
    std::string wire_;
    StatusCode code_;
};
using SharedStatusFrame = std::shared_ptr<const StatusFrame>;
class StatusDelivery final {
public:
    [[nodiscard]] const SharedStatusFrame& frame() const noexcept { return data_->frame; }
    [[nodiscard]] std::span<const users::SessionId> recipients() const noexcept { return data_->recipients; }
private:
    friend class StatusRouter;
    struct Data { users::SessionId sender; SharedStatusFrame frame; std::vector<users::SessionId> recipients; };
    struct Attempt { bool started{false}; };
    StatusDelivery(std::shared_ptr<const detail::StatusDomain> domain, std::shared_ptr<const Data> data)
        : domain_(std::move(domain)), data_(std::move(data)), attempt_(std::make_shared<Attempt>()) {}
    std::shared_ptr<const detail::StatusDomain> domain_;
    std::shared_ptr<const Data> data_;
    std::shared_ptr<Attempt> attempt_;
};
struct StatusDispatchReport {
    std::size_t queued{}, refused{}, canceled{};
    friend bool operator==(const StatusDispatchReport&, const StatusDispatchReport&) = default;
};
// Owner-thread B/D/E/F STA only. SessionManager must outlive this router.
// Fixed ready lifetime audience; no pending transfer/token correlation. Routed
// fatal severity belongs to peer context and never closes a hub connection here.
class StatusRouter final {
public:
    using Sink = std::function<bool(users::SessionId, const SharedStatusFrame&)>;
    explicit StatusRouter(const users::SessionManager& sessions) : sessions_(sessions) {}
    StatusRouter(const StatusRouter&) = delete;
    StatusRouter& operator=(const StatusRouter&) = delete;
    [[nodiscard]] std::variant<StatusDelivery, StatusRoutingError> prepare(users::SessionId sender, const Message& message) const;
    // Copies share one attempt; foreign/missing-sink errors do not consume it.
    // Capture immutable data before callbacks; no allocations during dispatch.
    // Recheck sender/recipient readiness and F selectors. Exceptions consume the
    // attempt and propagate; callbacks must not retry partial queue acceptance.
    [[nodiscard]] std::variant<StatusDispatchReport, StatusRoutingError> dispatch(const StatusDelivery& delivery, const Sink& sink) const;
    [[nodiscard]] bool cancel(const StatusDelivery& delivery) const noexcept;
private:
    [[nodiscard]] std::optional<StatusRoutingError> sender_error(users::SessionId sender, const Message& message) const noexcept;
    [[nodiscard]] bool matches(const users::Session& session, const Message& message) const noexcept;
    const users::SessionManager& sessions_;
    const std::shared_ptr<const detail::StatusDomain> domain_{std::make_shared<const detail::StatusDomain>()};
};
} // namespace dc24h::adc
