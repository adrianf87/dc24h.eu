#pragma once

#include "dc24h/adc/public_chat.hpp"
#include "dc24h/adc/private_chat.hpp"

namespace dc24h::adc {
enum class RoutingError {
    invalid_message, invalid_state, wrong_source, unsupported_type,
    private_broadcast, unsupported_group, recipient_unavailable
};
using RoutedMessage = std::variant<PublicBroadcast, PrivateDelivery>;
using RoutedFrame = std::variant<SharedPublicChatFrame, SharedPrivateChatFrame>;

// One MSG policy entry point. No fallback from private/unsupported scopes to
// public chat. Core binds plans to its routing domain and session lifetimes;
// LoginHub retains bounded FIFO queues, fault isolation and transport ownership.
class MessageRouter final {
public:
    using Sink = std::function<bool(users::SessionId, const RoutedFrame&)>;
    explicit MessageRouter(const chat::Core& core) noexcept : core_(core), public_(core), private_(core) {}
    MessageRouter(const MessageRouter&) = delete;
    MessageRouter& operator=(const MessageRouter&) = delete;
    [[nodiscard]] std::variant<RoutedMessage, RoutingError> prepare(users::SessionId sender, const Message& message) const;
    // One attempt. Acceptance means queued, not delivered/read. Sinks must not
    // throw or retry partially dispatched plans. No allocation after sink setup.
    [[nodiscard]] std::variant<chat::DispatchReport, chat::Error> dispatch(const RoutedMessage& message, const Sink& sink) const;
private:
    const chat::Core& core_;
    PublicChat public_;
    PrivateChat private_;
};
} // namespace dc24h::adc
