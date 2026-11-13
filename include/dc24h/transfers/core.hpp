#pragma once

#include "dc24h/transfers/request.hpp"
#include "dc24h/users/session_manager.hpp"
#include <functional>

namespace dc24h::transfers {
enum class Error { missing_sender, sender_not_ready, wrong_source, recipient_unavailable,
    self_connection, incompatible_connectivity, foreign_plan, missing_sink, already_consumed };
enum class State { prepared, dispatching, queued, refused, canceled, failed };
namespace detail {
struct Domain final {};
struct Attempt final { State state{State::prepared}; };
}
class Plan final {
public:
    [[nodiscard]] users::SessionId sender() const noexcept { return sender_; }
    [[nodiscard]] users::SessionId target() const noexcept { return target_; }
    [[nodiscard]] const std::string& source() const noexcept { return source_; }
    [[nodiscard]] const std::string& target_key() const noexcept { return target_key_; }
    [[nodiscard]] const Request& request() const noexcept { return request_; }
    [[nodiscard]] users::SessionId listener() const noexcept { return request_.kind() == Kind::connect_to_me ? sender_ : target_; }
    [[nodiscard]] users::SessionId connector() const noexcept { return request_.kind() == Kind::connect_to_me ? target_ : sender_; }
    [[nodiscard]] const users::AddressBytes& listener_address() const noexcept { return address_; }
    [[nodiscard]] State state() const noexcept { return attempt_ ? attempt_->state : State::canceled; }
private:
    friend class Core;
    Plan(std::shared_ptr<const detail::Domain> domain, users::SessionId sender, users::SessionId target,
         std::string_view source, std::string_view target_key, Request request, users::AddressBytes address);
    std::shared_ptr<const detail::Domain> domain_;
    users::SessionId sender_, target_;
    std::string source_, target_key_;
    Request request_;
    users::AddressBytes address_;
    std::shared_ptr<detail::Attempt> attempt_;
};
using Preparation = std::variant<Plan, Error, RequestError>;
// Owner-thread coordination foundation only. SessionManager must outlive Core.
// No ADC codec, wire routing, sockets, file bytes, timers or pending registry.
class Core final {
public:
    using Sink = std::function<bool(users::SessionId, const Plan&)>;
    explicit Core(const users::SessionManager& sessions) : sessions_(sessions) {}
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    Core(Core&&) = delete;
    Core& operator=(Core&&) = delete;
    [[nodiscard]] std::optional<Error> validate_sender(users::SessionId sender, std::string_view source) const noexcept;
    [[nodiscard]] std::optional<users::AddressBytes> tcp_address(users::SessionId owner, AddressFamily family) const noexcept;
    [[nodiscard]] bool owns(const Plan& plan) const noexcept { return plan.domain_ == domain_; }
    [[nodiscard]] Preparation prepare(users::SessionId sender, std::string_view source,
                                      std::string_view target, const RequestData& data) const;
    // Copies share one attempt. Foreign/missing-sink errors do not consume it.
    // All other dispatches consume before rechecks/callback. Accepted means
    // queued coordination only, never connected/transferred. Allocation-free
    // after sink construction. Sinks may close sessions; exceptions propagate
    // with state failed, so retrying cannot duplicate a partially queued action.
    [[nodiscard]] std::variant<State, Error> dispatch(const Plan& plan, const Sink& sink) const;
    [[nodiscard]] bool cancel(const Plan& plan) const noexcept;
private:
    [[nodiscard]] std::optional<users::AddressBytes> address(const users::Session& sender,
        const users::Session& target, const Request& request) const noexcept;
    const users::SessionManager& sessions_;
    const std::shared_ptr<const detail::Domain> domain_{std::make_shared<const detail::Domain>()};
};
} // namespace dc24h::transfers
