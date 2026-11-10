#pragma once

#include "dc24h/adc/msg.hpp"
#include "dc24h/chat/core.hpp"

#include <memory>
#include <utility>

namespace dc24h::adc {
enum class PublicChatError { invalid_message, unsupported_type, private_message, invalid_state, wrong_source };
// One checked public frame per publication. Only PublicChat can construct it;
// queues share immutable ownership and keep their own byte offsets/charges.
class PublicChatFrame final {
public:
    PublicChatFrame(const PublicChatFrame&) = delete;
    PublicChatFrame& operator=(const PublicChatFrame&) = delete;
    PublicChatFrame(PublicChatFrame&&) = delete;
    PublicChatFrame& operator=(PublicChatFrame&&) = delete;
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
private:
    friend class PublicChat;
    PublicChatFrame(Message message, std::string wire) : message_(std::move(message)), wire_(std::move(wire)) {}
    Message message_;
    std::string wire_;
};
using SharedPublicChatFrame = std::shared_ptr<const PublicChatFrame>;
class PublicBroadcast final {
public:
    [[nodiscard]] const chat::Broadcast& admission() const noexcept { return admission_; }
    [[nodiscard]] const SharedPublicChatFrame& frame() const noexcept { return frame_; }
private:
    friend class PublicChat;
    PublicBroadcast(chat::Broadcast admission, SharedPublicChatFrame frame)
        : admission_(std::move(admission)), frame_(std::move(frame)) {}
    chat::Broadcast admission_;
    SharedPublicChatFrame frame_;
};
// ADC public-chat policy only: BMSG without PM. Core owns lifecycle selection;
// the transport owner supplies bounded queues and isolates queue exceptions.
class PublicChat final {
public:
    using Sink = std::function<bool(users::SessionId, const SharedPublicChatFrame&)>;
    explicit PublicChat(const chat::Core& core) noexcept : core_(core) {}
    PublicChat(const PublicChat&) = delete;
    PublicChat& operator=(const PublicChat&) = delete;
    [[nodiscard]] std::variant<PublicBroadcast, PublicChatError> prepare(users::SessionId sender, const Message& message) const;
    // Dispatch is one attempt, not an acknowledgment or retry service. An empty
    // sink returns missing_sink. Other sink exceptions propagate, as in Core.
    [[nodiscard]] std::variant<chat::DispatchReport, chat::Error> dispatch(const PublicBroadcast& message, const Sink& sink) const;
private:
    const chat::Core& core_;
};
} // namespace dc24h::adc
