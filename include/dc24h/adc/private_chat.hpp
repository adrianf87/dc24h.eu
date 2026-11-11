#pragma once

#include "dc24h/adc/msg.hpp"
#include "dc24h/chat/core.hpp"
#include "dc24h/chat/settings.hpp"

#include <memory>
#include <utility>

namespace dc24h::adc {
enum class PrivateChatError {
    invalid_message, unsupported_type, unsupported_group, invalid_state, wrong_source, recipient_unavailable, text_limit, disabled
};
class PrivateChatFrame final {
public:
    PrivateChatFrame(const PrivateChatFrame&) = delete;
    PrivateChatFrame& operator=(const PrivateChatFrame&) = delete;
    PrivateChatFrame(PrivateChatFrame&&) = delete;
    PrivateChatFrame& operator=(PrivateChatFrame&&) = delete;
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
private:
    friend class PrivateChat;
    PrivateChatFrame(Message message, std::string wire) : message_(std::move(message)), wire_(std::move(wire)) {}
    Message message_;
    std::string wire_;
};
using SharedPrivateChatFrame = std::shared_ptr<const PrivateChatFrame>;
class PrivateDelivery final {
public:
    [[nodiscard]] const chat::Directed& admission() const noexcept { return admission_; }
    [[nodiscard]] const SharedPrivateChatFrame& frame() const noexcept { return frame_; }
private:
    friend class PrivateChat;
    PrivateDelivery(chat::Directed admission, SharedPrivateChatFrame frame)
        : admission_(std::move(admission)), frame_(std::move(frame)) {}
    chat::Directed admission_;
    SharedPrivateChatFrame frame_;
};
// DMSG: target only. EMSG: target and sender, once for a self-message.
// Optional PM must name the source for normal private conversations; groups
// remain unsupported. Unknown fields never enter the sanitized private frame.
class PrivateChat final {
public:
    using Sink = std::function<bool(users::SessionId, const SharedPrivateChatFrame&)>;
    explicit PrivateChat(const chat::Core& core, chat::Policy policy = {}) : core_(core), policy_(policy) { chat::validate_policy(policy_); }
    PrivateChat(const PrivateChat&) = delete;
    PrivateChat& operator=(const PrivateChat&) = delete;
    [[nodiscard]] std::variant<PrivateDelivery, PrivateChatError> prepare(users::SessionId sender, const Message& message) const;
    [[nodiscard]] std::variant<chat::DispatchReport, chat::Error> dispatch(const PrivateDelivery& message, const Sink& sink) const;
private:
    const chat::Core& core_;
    const chat::Policy policy_;
};
} // namespace dc24h::adc
