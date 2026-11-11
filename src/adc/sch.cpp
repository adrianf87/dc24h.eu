#include "dc24h/adc/sch.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/sid.hpp"
#include "dc24h/text/public_text.hpp"

#include <algorithm>
#include <array>

namespace dc24h::adc {
namespace {
struct Decoded {
    search::QueryData criteria;
    search::Selection selection;
};
std::optional<SchError> preflight(const Message& message) noexcept {
    if (message.kind != Command::sch || message.command != "SCH" ||
        (message.positional_count && *message.positional_count != 0)) { return SchError::invalid_message; }
    if (message.type != MessageType::broadcast && message.type != MessageType::feature &&
        message.type != MessageType::direct && message.type != MessageType::echo) { return SchError::invalid_type; }
    const bool directed = message.type == MessageType::direct || message.type == MessageType::echo;
    if (!message.source || !Sid::parse(*message.source) || directed != message.target.has_value() ||
        (message.target && !Sid::parse(*message.target)) ||
        (message.type == MessageType::feature ? message.features.empty() : !message.features.empty())) {
        return SchError::invalid_message;
    }
    if (message.parameters.size() > max_parameters || message.features.size() > search::max_selectors) { return SchError::field_limit; }
    std::size_t bytes = 0;
    for (const auto& parameter : message.parameters) {
        if (parameter.size() > max_sch_bytes - bytes) { return SchError::field_limit; }
        bytes += parameter.size();
    }
    for (std::size_t i = 0; i < message.features.size(); ++i) {
        const auto& name = message.features[i].name;
        if (name.size() != 4 || !std::all_of(name.begin(), name.begin()+3, [](char c) { return c >= 'A' && c <= 'Z'; }) ||
            !((name[3] >= 'A' && name[3] <= 'Z') || (name[3] >= '0' && name[3] <= '9'))) { return SchError::invalid_features; }
        for (std::size_t j = 0; j < i; ++j) {
            if (message.features[j].name == name) { return SchError::invalid_features; }
        }
    }
    return {};
}
std::variant<Decoded, SchError> decode(const Message& message) {
    Decoded result;
    auto& data = result.criteria;
    for (const auto& selector : message.features) {
        (selector.required ? result.selection.required_features : result.selection.excluded_features).push_back(selector.name);
    }
    std::array<bool, 6> seen{}; // GE, LE, EQ, TY, TO, TR are singular.
    bool criterion = false;
    for (const auto& parameter : message.parameters) {
        const auto field = parse_named_parameter(parameter);
        if (!field) { return SchError::invalid_message; }
        const auto name = field->name, value = field->value;
        if (name == "AN" || name == "NO") {
            if (data.include.size() + data.exclude.size() == search::max_terms) { return SchError::field_limit; }
            if (value.empty() || value.size() > search::max_term_bytes ||
                std::all_of(value.begin(), value.end(), [](char c) { return c == ' '; }) || !text::valid_public_text(value)) {
                return SchError::invalid_text;
            }
            (name == "AN" ? data.include : data.exclude).emplace_back(value); criterion = true;
        } else if (name == "EX") {
            if (data.extensions.size() == search::max_extensions) { return SchError::field_limit; }
            if (value.empty() || value.size() > search::max_extension_bytes || value.front() == '.' ||
                value.find_first_of("/\\ ") != std::string_view::npos || !text::valid_public_text(value)) { return SchError::invalid_extension; }
            data.extensions.emplace_back(value); criterion = true;
        } else {
            const std::array<std::string_view, 6> names{"GE","LE","EQ","TY","TO","TR"};
            const auto found = std::find(names.begin(), names.end(), name);
            if (found == names.end()) { continue; } // Unknown fields remain in the owning Message.
            const auto index = static_cast<std::size_t>(found-names.begin());
            if (seen[index]) { return SchError::duplicate_field; }
            seen[index] = true;
            if (index <= 3) {
                const auto number = parse_integer(value);
                if (!number || *number < 0) { return SchError::invalid_integer; }
                if (index == 0) { data.min_bytes = *number; }
                else if (index == 1) { data.max_bytes = *number; }
                else if (index == 2) { data.exact_bytes = *number; }
                else {
                    if (*number != 1 && *number != 2) { return SchError::invalid_file_type; }
                    data.type = *number == 1 ? search::ItemType::file : search::ItemType::directory;
                }
                criterion = true;
            } else if (index == 4) {
                if (value.empty() || value.size() > search::max_token_bytes || !text::valid_public_text(value)) { return SchError::invalid_token; }
                data.token = std::string(value);
            } else {
                data.tth = parse_tiger_base32(value);
                if (!data.tth) { return SchError::invalid_hash; }
                criterion = true;
            }
        }
    }
    if ((data.min_bytes && data.max_bytes && *data.min_bytes > *data.max_bytes) ||
        (data.exact_bytes && data.min_bytes && *data.exact_bytes < *data.min_bytes) ||
        (data.exact_bytes && data.max_bytes && *data.exact_bytes > *data.max_bytes)) { return SchError::invalid_size; }
    if (!criterion) { return SchError::no_criteria; }
    return result;
}
} // namespace
SchParseResult parse_sch(std::string_view body) {
    auto parsed = parse_line(body);
    if (const auto* error = std::get_if<ParseError>(&parsed)) { return *error; }
    auto* message = std::get_if<Message>(&parsed);
    if (!message) { return SchError::invalid_message; }
    if (const auto error = preflight(*message)) { return *error; }
    auto fields = decode(*message);
    if (const auto* error = std::get_if<SchError>(&fields)) { return *error; }
    auto& data = std::get<Decoded>(fields);
    return SchRequest(std::move(*message), std::move(data.criteria), std::move(data.selection));
}
std::variant<SchRequest, SchError> validate_sch(const Message& message) {
    if (const auto error = preflight(message)) { return *error; }
    // Shared serialization reparses a bounded frame, covering Unicode, headers,
    // escaping, unknown fields and deliberately inconsistent hand-built objects.
    if (!std::holds_alternative<std::string>(serialize(message))) { return SchError::invalid_message; }
    auto fields = decode(message);
    if (const auto* error = std::get_if<SchError>(&fields)) { return *error; }
    auto& data = std::get<Decoded>(fields);
    return SchRequest(message, std::move(data.criteria), std::move(data.selection));
}
std::variant<search::Query, search::QueryError, SchError> SchRequest::to_core_query() const {
    auto result = search::Query::create(criteria_);
    if (auto* query = std::get_if<search::Query>(&result)) { return std::move(*query); }
    return std::get<search::QueryError>(result);
}
std::variant<search::Filter, search::QueryError, SchError> SchRequest::to_filter() const {
    auto result = search::Filter::create(criteria_);
    if (auto* filter = std::get_if<search::Filter>(&result)) { return std::move(*filter); }
    return std::get<search::QueryError>(result);
}
} // namespace dc24h::adc
