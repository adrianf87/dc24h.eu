#include "dc24h/adc/res.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/sid.hpp"
#include "dc24h/text/public_text.hpp"

#include <algorithm>
#include <array>

namespace dc24h::adc {
namespace {
struct Decoded {
    search::ItemData item;
    std::optional<std::int64_t> slots;
    std::optional<std::string> token;
};
std::optional<ResError> preflight(const Message& message) noexcept {
    if (message.kind != Command::res || message.command != "RES" ||
        (message.positional_count && *message.positional_count != 0)) { return ResError::invalid_message; }
    if (message.type != MessageType::direct) { return ResError::invalid_type; }
    if (!message.source || !Sid::parse(*message.source) || !message.target || !Sid::parse(*message.target) ||
        !message.features.empty()) { return ResError::invalid_message; }
    if (message.parameters.size() > max_parameters) { return ResError::field_limit; }
    std::size_t bytes = 0;
    for (const auto& parameter : message.parameters) {
        if (parameter.size() > max_res_bytes - bytes) { return ResError::field_limit; }
        bytes += parameter.size();
    }
    return {};
}
bool valid_name(std::string_view name) {
    if (name.empty() || name.size() > search::max_item_name_bytes || !text::valid_public_text(name)) { return false; }
    // Permit both rooted and relative virtual share names. A trailing slash
    // denotes a directory; root alone, empty and dot components are invalid.
    if (name.front() == '/') { name.remove_prefix(1); }
    if (!name.empty() && name.back() == '/') { name.remove_suffix(1); }
    if (name.empty()) { return false; }
    while (true) {
        const auto slash = name.find('/');
        const auto part = name.substr(0, slash);
        if (part.empty() || part == "." || part == "..") { return false; }
        if (slash == std::string_view::npos) { return true; }
        name.remove_prefix(slash + 1);
    }
}
std::variant<Decoded, ResError> decode(const Message& message) {
    Decoded result;
    constexpr std::array<std::string_view, 5> names{"FN", "SI", "SL", "TO", "TR"};
    std::array<bool, names.size()> seen{};
    for (const auto& parameter : message.parameters) {
        const auto field = parse_named_parameter(parameter);
        if (!field) { return ResError::invalid_message; }
        const auto found = std::find(names.begin(), names.end(), field->name);
        if (found == names.end()) { continue; } // Preserve unknown extensions unchanged.
        const auto index = static_cast<std::size_t>(found - names.begin());
        if (seen[index]) { return ResError::duplicate_field; }
        seen[index] = true;
        const auto value = field->value;
        switch (index) {
        case 0:
            if (!valid_name(value)) { return ResError::invalid_name; }
            result.item.name = value;
            result.item.type = value.back() == '/' ? search::ItemType::directory : search::ItemType::file;
            break;
        case 1: case 2: {
            const auto number = parse_integer(value);
            if (!number || *number < 0) { return ResError::invalid_integer; }
            (index == 1 ? result.item.bytes : result.slots) = *number;
            break;
        }
        case 3:
            if (value.empty() || value.size() > search::max_token_bytes || !text::valid_public_text(value)) {
                return ResError::invalid_token;
            }
            result.token = value;
            break;
        case 4:
            result.item.tth = parse_tiger_base32(value);
            if (!result.item.tth) { return ResError::invalid_hash; }
            break;
        }
    }
    if (!seen[0] || !seen[1] || (result.item.type == search::ItemType::file && !seen[4])) {
        return ResError::missing_field;
    }
    if (result.item.type == search::ItemType::directory && seen[4]) { return ResError::invalid_hash; }
    // TO is optional for a tokenless SCH; a supplied token is strictly checked.
    return result;
}
} // namespace
ResParseResult parse_res(std::string_view body) {
    auto parsed = parse_line(body);
    if (const auto* error = std::get_if<ParseError>(&parsed)) { return *error; }
    auto* message = std::get_if<Message>(&parsed);
    if (!message) { return ResError::invalid_message; }
    if (const auto error = preflight(*message)) { return *error; }
    auto fields = decode(*message);
    if (const auto* error = std::get_if<ResError>(&fields)) { return *error; }
    auto& data = std::get<Decoded>(fields);
    return ResRequest(std::move(*message), std::move(data.item), data.slots, std::move(data.token));
}
std::variant<ResRequest, ResError> validate_res(const Message& message) {
    if (const auto error = preflight(message)) { return *error; }
    if (!std::holds_alternative<std::string>(serialize(message))) { return ResError::invalid_message; }
    auto fields = decode(message);
    if (const auto* error = std::get_if<ResError>(&fields)) { return *error; }
    auto& data = std::get<Decoded>(fields);
    return ResRequest(message, std::move(data.item), data.slots, std::move(data.token));
}
} // namespace dc24h::adc
