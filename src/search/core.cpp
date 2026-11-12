#include "dc24h/search/core.hpp"

#include <algorithm>

namespace dc24h::search {
namespace {
bool feature_name(std::string_view name) noexcept {
    return name.size() == 4 && std::all_of(name.begin(), name.begin()+3, [](char c) { return c >= 'A' && c <= 'Z'; }) &&
        ((name[3] >= 'A' && name[3] <= 'Z') || (name[3] >= '0' && name[3] <= '9'));
}
bool valid_selection(const Selection& selection) noexcept {
    if (selection.required_features.size() > max_selectors ||
        selection.excluded_features.size() > max_selectors - selection.required_features.size()) { return false; }
    for (const auto* names : {&selection.required_features, &selection.excluded_features}) {
        for (std::size_t i = 0; i < names->size(); ++i) {
            if (!feature_name((*names)[i]) || std::find(names->begin(), names->begin()+static_cast<std::ptrdiff_t>(i), (*names)[i]) !=
                names->begin()+static_cast<std::ptrdiff_t>(i)) { return false; }
        }
    }
    for (const auto& name : selection.required_features) {
        if (std::find(selection.excluded_features.begin(), selection.excluded_features.end(), name) != selection.excluded_features.end()) { return false; }
    }
    return true;
}
bool eligible(const users::Session& model, const Selection& selection) noexcept {
    if (!model.ready() || !model.user()) { return false; }
    const auto& features = model.user()->profile().data().features;
    const auto has = [&](const std::string& name) { return std::find(features.begin(), features.end(), name) != features.end(); };
    return std::all_of(selection.required_features.begin(), selection.required_features.end(), has) &&
        std::none_of(selection.excluded_features.begin(), selection.excluded_features.end(), has);
}
} // namespace
std::optional<Error> Core::validate_sender(users::SessionId sender, std::string_view source) const noexcept {
    const auto* model = sessions_.find(sender);
    if (!model) { return Error::missing_sender; }
    if (!model->ready() || !model->identity()) { return Error::sender_not_ready; }
    if (model->identity()->sid() != source) { return Error::wrong_source; }
    return {};
}
std::optional<users::SessionId> Core::resolve_target(std::string_view source) const noexcept {
    const auto* model = sessions_.by_sid(source);
    return model && model->ready() ? std::optional{model->id()} : std::nullopt;
}
Preparation Core::prepare(users::SessionId sender, std::string_view source, const QueryData& data, const Selection& selection, const Audience& audience) const {
    if (const auto error = validate_sender(sender, source)) { return *error; }
    if (!valid_selection(selection)) { return Error::invalid_selection; }
    if ((audience.scope != Scope::broadcast && audience.scope != Scope::direct && audience.scope != Scope::echo) ||
        (audience.scope != Scope::broadcast) != audience.target.has_value()) { return Error::invalid_audience; }
    auto query = Query::create(data);
    if (const auto* error = std::get_if<QueryError>(&query)) { return *error; }
    std::vector<users::SessionId> recipients;
    if (audience.target) {
        const auto* target = sessions_.find(*audience.target);
        if (!target || !eligible(*target,selection)) { return Error::recipient_unavailable; }
        recipients.push_back(*audience.target);
        if (audience.scope == Scope::echo && sender != *audience.target && eligible(*sessions_.find(sender),selection)) {
            recipients.push_back(sender);
        }
        return Plan(domain_,sender,source,std::get<Query>(std::move(query)),selection,std::move(recipients));
    }
    recipients.reserve(sessions_.statistics().online());
    std::uint64_t after = 0;
    for (;;) {
        const auto page = sessions_.public_users(after, users::SessionManager::max_page_size);
        if (page.empty()) { break; }
        for (const auto id : page) {
            const auto* model = sessions_.find(id);
            if (model && eligible(*model, selection)) { recipients.push_back(id); }
        }
        after = page.back().value();
    }
    return Plan(domain_, sender, source, std::get<Query>(std::move(query)), selection, std::move(recipients));
}
std::variant<DispatchReport, Error> Core::dispatch(const Plan& plan, const Sink& sink) const {
    if (plan.domain_ != domain_) { return Error::foreign_plan; }
    if (const auto error = validate_sender(plan.sender(), plan.source())) { return *error; }
    if (!sink) { return Error::missing_sink; }
    DispatchReport report;
    for (std::size_t i = 0; i < plan.recipients().size(); ++i) {
        if (validate_sender(plan.sender(), plan.source())) { report.canceled = plan.recipients().size()-i; break; }
        const auto recipient = plan.recipients()[i];
        const auto* model = sessions_.find(recipient);
        if (!model || !eligible(*model, plan.selection())) { ++report.unavailable; continue; }
        if (sink(recipient, plan)) { ++report.queued; }
        else { ++report.refused; }
    }
    return report;
}
StartResult Core::start(users::SessionId sender, std::string_view source, const QueryData& data,
                        users::TimePoint now, const Selection& selection, const Audience& audience) {
    if (const auto error = validate_sender(sender,source)) { return *error; }
    if (!data.token) { return TokenError::missing_token; }
    auto prepared = prepare(sender,source,data,selection,audience);
    if (const auto* error = std::get_if<Error>(&prepared)) { return *error; }
    if (const auto* error = std::get_if<QueryError>(&prepared)) { return *error; }
    auto handle = tokens_.activate(sender,*data.token,now);
    if (const auto* error = std::get_if<TokenError>(&handle)) { return *error; }
    return ActivePlan(std::get<Plan>(std::move(prepared)),std::get<TokenHandle>(std::move(handle)));
}
std::variant<DispatchReport, Error, TokenError> Core::dispatch(const ActivePlan& search, const Sink& sink, users::TimePoint now) {
    const auto& plan = search.plan();
    if (plan.domain_ != domain_) { return Error::foreign_plan; }
    if (const auto error = validate_sender(plan.sender(),plan.source())) { return *error; }
    if (!tokens_.active(search.handle(),now)) { return TokenError::inactive_search; }
    if (!sink) { return Error::missing_sink; }
    DispatchReport report;
    for (std::size_t i = 0; i < plan.recipients().size(); ++i) {
        if (validate_sender(plan.sender(),plan.source()) || !tokens_.active(search.handle(),now)) {
            report.canceled = plan.recipients().size()-i; break;
        }
        const auto recipient = plan.recipients()[i];
        const auto* model = sessions_.find(recipient);
        if (!model || !eligible(*model,plan.selection())) { ++report.unavailable; continue; }
        if (sink(recipient,plan)) { ++report.queued; }
        else { ++report.refused; }
    }
    return report;
}
bool Core::complete(const ActivePlan& search) noexcept {
    return search.plan().domain_ == domain_ && tokens_.cancel(search.handle());
}
} // namespace dc24h::search
