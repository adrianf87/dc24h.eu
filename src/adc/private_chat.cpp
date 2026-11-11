#include "dc24h/adc/private_chat.hpp"
#include "dc24h/adc/chat_validation.hpp"

namespace dc24h::adc {
std::variant<PrivateDelivery, PrivateChatError> PrivateChat::prepare(users::SessionId sender, const Message& message) const {
    const auto checked=validate_chat(core_,sender,message);
    const auto* view=std::get_if<MsgView>(&checked);
    if (!view) {
        switch (std::get<ChatValidationError>(checked)) {
        case ChatValidationError::invalid_state: return PrivateChatError::invalid_state;
        case ChatValidationError::wrong_source: return PrivateChatError::wrong_source;
        case ChatValidationError::unsupported_type: return PrivateChatError::unsupported_type;
        default: return PrivateChatError::invalid_message;
        }
    }
    if (message.type!=MessageType::direct && message.type!=MessageType::echo) { return PrivateChatError::unsupported_type; }
    if (view->private_group && view->private_group!=Sid::parse(*message.source)) { return PrivateChatError::unsupported_group; }
    auto prepared=core_.prepare_direct(sender,*message.source,*message.target,view->text,
        view->third_person,message.type==MessageType::echo);
    if (const auto* error=std::get_if<chat::Error>(&prepared)) {
        switch (*error) {
        case chat::Error::missing_sender: case chat::Error::sender_not_ready: return PrivateChatError::invalid_state;
        case chat::Error::wrong_source: return PrivateChatError::wrong_source;
        case chat::Error::recipient_unavailable: return PrivateChatError::recipient_unavailable;
        default: return PrivateChatError::invalid_message;
        }
    }
    auto admission=std::get<chat::Directed>(std::move(prepared));
    Message safe{message.type,"MSG",Command::msg,admission.source(),admission.target_key(),{},
        make_msg_parameters(admission.text(), view->flags()),1};
    auto encoded=serialize_msg(safe);
    auto* wire=std::get_if<std::string>(&encoded);
    if (!wire) { return PrivateChatError::invalid_message; }
    auto frame=SharedPrivateChatFrame(new PrivateChatFrame(std::move(safe),std::move(*wire)));
    return PrivateDelivery(std::move(admission),std::move(frame));
}
std::variant<chat::DispatchReport, chat::Error> PrivateChat::dispatch(const PrivateDelivery& message, const Sink& sink) const {
    if (!sink) { return chat::Error::missing_sink; }
    if (!message.frame()) { return chat::Error::invalid_text; }
    return core_.dispatch(message.admission(),[&](users::SessionId recipient,const chat::Directed&) {
        return sink(recipient,message.frame());
    });
}
} // namespace dc24h::adc
