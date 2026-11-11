#include "dc24h/adc/public_chat.hpp"
#include "dc24h/adc/chat_validation.hpp"

namespace dc24h::adc {
std::variant<PublicBroadcast, PublicChatError> PublicChat::prepare(users::SessionId sender, const Message& message) const {
    const auto validated=validate_chat(core_,sender,message);
    const auto* view=std::get_if<MsgView>(&validated);
    if (!view) {
        switch (std::get<ChatValidationError>(validated)) {
        case ChatValidationError::invalid_state: return PublicChatError::invalid_state;
        case ChatValidationError::wrong_source: return PublicChatError::wrong_source;
        case ChatValidationError::unsupported_type: return PublicChatError::unsupported_type;
        default: return PublicChatError::invalid_message;
        }
    }
    if (message.type!=MessageType::broadcast) { return PublicChatError::unsupported_type; }
    if (view->private_group) { return PublicChatError::private_message; }
    auto prepared=core_.prepare(sender,*message.source,view->text,view->third_person);
    if (const auto* error=std::get_if<chat::Error>(&prepared)) {
        switch (*error) {
        case chat::Error::missing_sender: case chat::Error::sender_not_ready: return PublicChatError::invalid_state;
        case chat::Error::wrong_source: return PublicChatError::wrong_source;
        default: return PublicChatError::invalid_message;
        }
    }
    auto admission=std::get<chat::Broadcast>(std::move(prepared));
    // Build a fresh allowlisted frame; private/unknown data never reaches public
    // output. Only source, text and canonical ME remain.
    Message safe{MessageType::broadcast,"MSG",Command::msg,admission.source(),{},{},
        make_msg_parameters(admission.text(), view->flags()),1};
    auto encoded=serialize_msg(safe);
    auto* wire=std::get_if<std::string>(&encoded);
    if (!wire) { return PublicChatError::invalid_message; }
    auto frame=SharedPublicChatFrame(new PublicChatFrame(std::move(safe),std::move(*wire)));
    return PublicBroadcast(std::move(admission),std::move(frame));
}
std::variant<chat::DispatchReport, chat::Error> PublicChat::dispatch(const PublicBroadcast& message, const Sink& sink) const {
    if (!sink) { return chat::Error::missing_sink; }
    if (!message.frame()) { return chat::Error::invalid_text; } // Moved-from plan.
    return core_.dispatch(message.admission(),[&](users::SessionId recipient,const chat::Broadcast&) {
        return sink(recipient,message.frame());
    });
}
} // namespace dc24h::adc
