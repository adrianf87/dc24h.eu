#include "dc24h/chat/core.hpp"
#include "dc24h/text/public_text.hpp"

namespace dc24h::chat {
std::optional<Error> Core::check_sender(users::SessionId sender, std::string_view source) const noexcept {
    const auto* model=sessions_.find(sender);
    if (!model) { return Error::missing_sender; }
    if (!model->ready() || !model->identity()) { return Error::sender_not_ready; }
    if (source!=model->identity()->sid()) { return Error::wrong_source; }
    return {};
}
std::variant<Broadcast, Error> Core::prepare(users::SessionId sender, std::string_view source,
        std::string_view text, std::optional<bool> third_person) const {
    if (const auto error=check_sender(sender,source)) { return *error; }
    if (!text::valid_chat_text(text)) { return Error::invalid_text; }
    std::vector<users::SessionId> recipients;
    recipients.reserve(sessions_.statistics().online());
    std::uint64_t after=0;
    for (;;) {
        const auto page=sessions_.public_users(after,users::SessionManager::max_page_size);
        if (page.empty()) { break; }
        for (const auto id:page) {
            const auto* model=sessions_.find(id);
            if (model && model->ready()) { recipients.push_back(id); }
        }
        after=page.back().value();
    }
    return Broadcast(sender,source,text,third_person,std::move(recipients));
}
std::variant<DispatchReport, Error> Core::dispatch(const Broadcast& message, const Sink& sink) const {
    if (const auto error=check_sender(message.sender(),message.source())) { return *error; }
    if (!sink) { return Error::missing_sink; }
    DispatchReport report;
    for (const auto id:message.recipients()) {
        const auto* model=sessions_.find(id);
        if (!model || !model->ready()) { ++report.unavailable;continue; }
        if (sink(id,message)) { ++report.queued; }
        else { ++report.refused; }
    }
    return report;
}
} // namespace dc24h::chat
