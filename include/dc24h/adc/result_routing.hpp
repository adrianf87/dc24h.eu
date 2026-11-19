#pragma once

#include "dc24h/adc/res.hpp"
#include "dc24h/search/core.hpp"

namespace dc24h::adc {
enum class ResultRoutingError {
    invalid_state, wrong_source, invalid_message, recipient_unavailable,
    inactive_search, foreign_plan, missing_sink, already_dispatched
};
namespace detail { struct ResultDomain final {}; }
class ResultFrame final {
public:
    class Passkey { friend class ResultRouter; Passkey() noexcept = default; };
    ResultFrame(const ResultFrame&) = delete;
    ResultFrame& operator=(const ResultFrame&) = delete;
    ResultFrame(ResultFrame&&) = delete;
    ResultFrame& operator=(ResultFrame&&) = delete;
    ResultFrame(Passkey, Message message, std::string wire) : message_(std::move(message)), wire_(std::move(wire)) {}
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
private:
    Message message_;
    std::string wire_;
};
using SharedResultFrame = std::shared_ptr<const ResultFrame>;
class RoutedResult final {
public:
    [[nodiscard]] users::SessionId sender() const noexcept { return sender_; }
    [[nodiscard]] users::SessionId recipient() const noexcept { return recipient_; }
    [[nodiscard]] const SharedResultFrame& frame() const noexcept { return frame_; }
    [[nodiscard]] const std::optional<search::ActiveSearch>& search() const noexcept { return search_; }
private:
    friend class ResultRouter;
    struct Attempt { bool started{false}; };
    RoutedResult(std::shared_ptr<const detail::ResultDomain> domain, users::SessionId sender,
                 users::SessionId recipient, SharedResultFrame frame, std::optional<std::string> token,
                 std::optional<search::ActiveSearch> search, std::shared_ptr<Attempt> attempt)
        : domain_(std::move(domain)), sender_(sender), recipient_(recipient), frame_(std::move(frame)),
          token_(std::move(token)), search_(search), attempt_(std::move(attempt)) {}
    std::shared_ptr<const detail::ResultDomain> domain_;
    users::SessionId sender_, recipient_;
    SharedResultFrame frame_;
    std::optional<std::string> token_;
    std::optional<search::ActiveSearch> search_;
    std::shared_ptr<Attempt> attempt_;
};
using ResultDispatch = std::variant<search::DispatchReport, ResultRoutingError>;
// Owner-thread DRES target-only routing. Supplied TO must identify the target's
// active search; absent TO remains uncorrelated. No responder/file authentication,
// result index, UDP, socket or queue I/O. Core and its sessions must outlive us.
class ResultRouter final {
public:
    using Sink = std::function<bool(users::SessionId, const SharedResultFrame&)>;
    explicit ResultRouter(search::Core& core) : core_(core) {}
    ResultRouter(const ResultRouter&) = delete;
    ResultRouter& operator=(const ResultRouter&) = delete;
    [[nodiscard]] std::variant<RoutedResult, ResultRoutingError> prepare(
        users::SessionId sender, const Message& message, users::TimePoint now);
    // Copies share one attempt. Foreign/missing-sink errors do not consume it.
    // No allocation after sink construction; recheck sender, recipient lifetime
    // and optional search generation. A false sink means refused, not delivered.
    // Sinks must not reenter/throw; exceptions propagate after a consumed attempt.
    [[nodiscard]] ResultDispatch dispatch(const RoutedResult& result, const Sink& sink, users::TimePoint now);
private:
    search::Core& core_;
    const std::shared_ptr<const detail::ResultDomain> domain_{std::make_shared<const detail::ResultDomain>()};
};
} // namespace dc24h::adc
