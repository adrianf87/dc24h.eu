#include "dc24h/adc/status_routing.hpp"
#include "dc24h/adc/serializer.hpp"
#include <algorithm>

namespace dc24h::adc {
std::optional<StatusRoutingError> StatusRouter::sender_error(users::SessionId sender, const Message& message) const noexcept {
    const auto* model = sessions_.find(sender);
    if (!model || !model->ready() || !model->user()) { return StatusRoutingError::invalid_state; }
    if (!message.source || model->identity()->sid() != *message.source) { return StatusRoutingError::wrong_source; }
    return {};
}
bool StatusRouter::matches(const users::Session& session, const Message& message) const noexcept {
    if (!session.ready() || !session.user()) { return false; }
    if (message.type != MessageType::feature) { return true; }
    const auto& features = session.user()->profile().data().features;
    for (const auto& selector : message.features) {
        const bool has = std::find(features.begin(), features.end(), selector.name) != features.end();
        if (has != selector.required) { return false; }
    }
    return true;
}
std::variant<StatusDelivery, StatusRoutingError> StatusRouter::prepare(users::SessionId sender, const Message& message) const {
    if (const auto error = sender_error(sender, message)) { return *error; }
    if (message.type != MessageType::broadcast && message.type != MessageType::direct &&
        message.type != MessageType::echo && message.type != MessageType::feature) { return StatusRoutingError::invalid_message; }
    const auto validated = validate_status(message);
    const auto* status = std::get_if<StatusView>(&validated);
    if (!status) { return StatusRoutingError::invalid_message; }
    std::vector<users::SessionId> recipients;
    if (message.type == MessageType::direct || message.type == MessageType::echo) {
        const auto* target = sessions_.by_sid(*message.target);
        if (!target || !matches(*target, message)) { return StatusRoutingError::recipient_unavailable; }
        recipients.push_back(target->id());
        if (message.type == MessageType::echo && target->id() != sender) { recipients.push_back(sender); }
    } else {
        std::uint64_t cursor = 0;
        for (;;) {
            const auto page = sessions_.public_users(cursor, users::SessionManager::max_page_size);
            if (page.empty()) { break; }
            for (const auto id : page) { if (matches(*sessions_.find(id), message)) { recipients.push_back(id); } }
            cursor = page.back().value();
        }
    }
    auto safe = public_status(message);
    auto wire = std::get<std::string>(serialize(safe));
    auto frame = SharedStatusFrame(new StatusFrame(std::move(safe), std::move(wire), status->code));
    auto data = std::make_shared<const StatusDelivery::Data>(StatusDelivery::Data{sender, std::move(frame), std::move(recipients)});
    return StatusDelivery(domain_, std::move(data));
}
std::variant<StatusDispatchReport, StatusRoutingError> StatusRouter::dispatch(const StatusDelivery& delivery, const Sink& sink) const {
    if (delivery.domain_ != domain_) { return StatusRoutingError::foreign_plan; }
    if (!delivery.data_ || !delivery.attempt_) { return StatusRoutingError::invalid_message; }
    if (!sink) { return StatusRoutingError::missing_sink; }
    if (delivery.attempt_->started) { return StatusRoutingError::already_dispatched; }
    const auto data = delivery.data_;
    const auto attempt = delivery.attempt_;
    attempt->started = true;
    if (const auto error = sender_error(data->sender, data->frame->message())) { return *error; }
    StatusDispatchReport report;
    for (const auto id : data->recipients) {
        const auto* target = sessions_.find(id);
        if (sender_error(data->sender, data->frame->message()) || !target || !matches(*target, data->frame->message()) ||
            (data->frame->message().target && id != data->sender && target->identity()->sid() != *data->frame->message().target)) {
            ++report.canceled; continue;
        }
        if (sink(id, data->frame)) { ++report.queued; } else { ++report.refused; }
    }
    return report;
}
bool StatusRouter::cancel(const StatusDelivery& delivery) const noexcept {
    if (delivery.domain_ != domain_ || !delivery.attempt_ || delivery.attempt_->started) { return false; }
    delivery.attempt_->started = true; return true;
}
} // namespace dc24h::adc
