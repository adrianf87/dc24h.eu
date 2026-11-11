#pragma once

#include "dc24h/adc/msg.hpp"
#include "dc24h/chat/core.hpp"

namespace dc24h::adc {
inline constexpr std::string_view hub_welcome_text = "Welcome to dc24h.eu. Guest access is enabled.";
enum class HubMessageError { invalid_text, frame_too_long, recipient_unavailable, missing_sink, foreign_plan, invalid_plan };

// Only trusted hub code creates these packets. No client Message or source SID
// is accepted. System output contains one text parameter and no PM/ME metadata.
class HubMessageFrame final {
public:
    HubMessageFrame(const HubMessageFrame&) = delete;
    HubMessageFrame& operator=(const HubMessageFrame&) = delete;
    [[nodiscard]] const Message& message() const noexcept { return message_; }
    [[nodiscard]] const std::string& wire() const noexcept { return wire_; }
private:
    friend std::variant<std::shared_ptr<const HubMessageFrame>, HubMessageError> make_hub_message(std::string_view);
    HubMessageFrame(Message message, std::string wire) : message_(std::move(message)), wire_(std::move(wire)) {}
    Message message_;
    std::string wire_;
};
using SharedHubMessageFrame = std::shared_ptr<const HubMessageFrame>;
[[nodiscard]] std::variant<SharedHubMessageFrame, HubMessageError> make_hub_message(std::string_view text);

namespace detail { struct HubMessageDomain final {}; }
class HubDelivery final {
public:
    [[nodiscard]] const SharedHubMessageFrame& frame() const noexcept { return frame_; }
    [[nodiscard]] const std::vector<users::SessionId>& recipients() const noexcept { return recipients_; }
private:
    friend class HubMessages;
    HubDelivery(std::shared_ptr<const detail::HubMessageDomain> domain, SharedHubMessageFrame frame,
                std::vector<users::SessionId> recipients)
        : domain_(std::move(domain)), frame_(std::move(frame)), recipients_(std::move(recipients)) {}
    std::shared_ptr<const detail::HubMessageDomain> domain_;
    SharedHubMessageFrame frame_;
    std::vector<users::SessionId> recipients_;
};

// Single reactor owner. Snapshot ready lifetimes, then recheck each at dispatch.
// No fake chat sender, SID lookup, persistent history, sockets or SQL.
class HubMessages final {
public:
    using Sink = std::function<bool(users::SessionId, const SharedHubMessageFrame&)>;
    explicit HubMessages(const users::SessionManager& sessions) : sessions_(sessions) {}
    HubMessages(const HubMessages&) = delete;
    HubMessages& operator=(const HubMessages&) = delete;
    [[nodiscard]] std::variant<HubDelivery, HubMessageError> prepare(std::string_view text,
        std::optional<users::SessionId> target = {}) const;
    // One attempt: accepted means queued, not delivered. Construct sink first;
    // dispatch allocates nothing. Sinks may close sessions but must not throw or
    // retry a partially published plan. Foreign/moved plans cannot deliver.
    [[nodiscard]] std::variant<chat::DispatchReport, HubMessageError> dispatch(
        const HubDelivery& plan, const Sink& sink) const;
private:
    const users::SessionManager& sessions_;
    const std::shared_ptr<const detail::HubMessageDomain> domain_{std::make_shared<const detail::HubMessageDomain>()};
};
} // namespace dc24h::adc
