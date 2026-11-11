#include "dc24h/adc/message_routing.hpp"

#include <type_traits>

namespace dc24h::adc {
std::variant<RoutedMessage, RoutingError> MessageRouter::prepare(users::SessionId sender, const Message& message) const {
    // Comparing the scope does not inspect optional headers. Each adapter calls
    // the shared validator before dereferencing or preparing a delivery plan.
    if (message.type==MessageType::direct || message.type==MessageType::echo) {
        auto result=private_.prepare(sender,message);
        if (auto* plan=std::get_if<PrivateDelivery>(&result)) { return RoutedMessage(std::move(*plan)); }
        switch (std::get<PrivateChatError>(result)) {
        case PrivateChatError::invalid_state: return RoutingError::invalid_state;
        case PrivateChatError::wrong_source: return RoutingError::wrong_source;
        case PrivateChatError::unsupported_type: return RoutingError::unsupported_type;
        case PrivateChatError::unsupported_group: return RoutingError::unsupported_group;
        case PrivateChatError::recipient_unavailable: return RoutingError::recipient_unavailable;
        case PrivateChatError::invalid_message: return RoutingError::invalid_message;
        case PrivateChatError::text_limit: return RoutingError::text_limit;
        case PrivateChatError::disabled: return RoutingError::disabled;
        }
    } else {
        auto result=public_.prepare(sender,message);
        if (auto* plan=std::get_if<PublicBroadcast>(&result)) { return RoutedMessage(std::move(*plan)); }
        switch (std::get<PublicChatError>(result)) {
        case PublicChatError::invalid_state: return RoutingError::invalid_state;
        case PublicChatError::wrong_source: return RoutingError::wrong_source;
        case PublicChatError::unsupported_type: return RoutingError::unsupported_type;
        case PublicChatError::private_message: return RoutingError::private_broadcast;
        case PublicChatError::invalid_message: return RoutingError::invalid_message;
        case PublicChatError::text_limit: return RoutingError::text_limit;
        case PublicChatError::disabled: return RoutingError::disabled;
        }
    }
    return RoutingError::invalid_message;
}
std::variant<chat::DispatchReport, chat::Error> MessageRouter::dispatch(const RoutedMessage& message, const Sink& sink) const {
    if (!sink) { return chat::Error::missing_sink; }
    return std::visit([&](const auto& plan) -> std::variant<chat::DispatchReport, chat::Error> {
        const RoutedFrame frame=plan.frame();
        const auto deliver=[&sink,&frame](users::SessionId recipient,const auto&) { return sink(recipient,frame); };
        if constexpr (std::is_same_v<std::decay_t<decltype(plan)>,PublicBroadcast>) { return public_.dispatch(plan,deliver); }
        else { return private_.dispatch(plan,deliver); }
    },message);
}
} // namespace dc24h::adc
