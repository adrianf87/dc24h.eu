#include "dc24h/adc/hub_messages.hpp"
#include "dc24h/adc/shared_frame.hpp"
#include "dc24h/text/public_text.hpp"

namespace dc24h::adc {
std::variant<SharedHubMessageFrame, HubMessageError> make_hub_message(std::string_view text) {
    if (text.size() > max_frame_bytes) { return HubMessageError::frame_too_long; }
    if (!text::valid_chat_text(text)) { return HubMessageError::invalid_text; }
    Message message{MessageType::info, "MSG", Command::msg, {}, {}, {}, {std::string(text)}, 1};
    auto encoded = serialize_msg(message);
    auto* wire = std::get_if<std::string>(&encoded);
    if (!wire) {
        return std::get<MsgError>(encoded) == MsgError::frame_too_long ?
            HubMessageError::frame_too_long : HubMessageError::invalid_text;
    }
    return share_frame<HubMessageFrame>(HubMessageFrame::Passkey{}, std::move(message), std::move(*wire));
}

std::variant<HubDelivery, HubMessageError> HubMessages::prepare(std::string_view text,
        std::optional<users::SessionId> target) const {
    if (target) {
        const auto* model = sessions_.find(*target);
        if (!model || !model->ready()) { return HubMessageError::recipient_unavailable; }
    }
    auto prepared = make_hub_message(text);
    if (const auto* error = std::get_if<HubMessageError>(&prepared)) { return *error; }
    std::vector<users::SessionId> recipients;
    if (target) { recipients.push_back(*target); }
    else {
        recipients.reserve(sessions_.statistics().online());
        std::uint64_t after = 0;
        for (;;) {
            const auto page = sessions_.public_users(after, users::SessionManager::max_page_size);
            if (page.empty()) { break; }
            for (const auto id : page) {
                const auto* model = sessions_.find(id);
                if (model && model->ready()) { recipients.push_back(id); }
            }
            after = page.back().value();
        }
    }
    return HubDelivery(domain_, std::get<SharedHubMessageFrame>(std::move(prepared)), std::move(recipients));
}

std::variant<chat::DispatchReport, HubMessageError> HubMessages::dispatch(const HubDelivery& plan, const Sink& sink) const {
    if (!plan.frame()) { return HubMessageError::invalid_plan; }
    if (plan.domain_ != domain_) { return HubMessageError::foreign_plan; }
    if (!sink) { return HubMessageError::missing_sink; }
    chat::DispatchReport report;
    for (const auto id : plan.recipients()) {
        const auto* model = sessions_.find(id);
        if (!model || !model->ready()) { ++report.unavailable; continue; }
        if (sink(id, plan.frame())) { ++report.queued; }
        else { ++report.refused; }
    }
    return report;
}
} // namespace dc24h::adc
