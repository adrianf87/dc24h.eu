#include "dc24h/adc/status.hpp"
#include "dc24h/adc/serializer.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace dc24h::adc {
namespace {
bool fourcc(std::string_view value) noexcept {
    return value.size() == 4 && value[0] >= 'A' && value[0] <= 'Z' &&
        std::all_of(value.begin()+1, value.end(), [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        });
}
bool known_status_field(std::string_view name) noexcept {
    constexpr std::string_view names[]{"FC", "FM", "FB", "TL", "TO", "PR", "I4", "I6"};
    return std::find(std::begin(names), std::end(names), name) != std::end(names);
}
bool ip(std::string_view value, bool v6) {
    if (value.empty() || value.size() >= INET6_ADDRSTRLEN) { return false; }
    std::array<unsigned char,16> bytes{};
    return ::inet_pton(v6 ? AF_INET6 : AF_INET, std::string(value).c_str(), bytes.data()) == 1;
}
bool field_name(std::string_view name) noexcept {
    const auto parsed = parse_named_parameter(name);
    return parsed && name.size() == 2;
}
bool text_atom(std::string_view value) noexcept {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return c > 32 && c < 127; });
}
} // namespace

std::optional<StatusCode> StatusCode::parse(std::string_view value) noexcept {
    if (value.size() != 3 || value[0] < '0' || value[0] > '2' ||
        value[1] < '0' || value[1] > '9' || value[2] < '0' || value[2] > '9') { return std::nullopt; }
    const auto error = static_cast<unsigned>((value[1]-'0')*10 + value[2]-'0');
    if (value[0] == '0' && error != 0) { return std::nullopt; }
    return StatusCode(static_cast<Severity>(value[0]-'0'), error);
}
std::string StatusCode::str() const {
    return {static_cast<char>('0'+static_cast<int>(severity_)),
            static_cast<char>('0'+error_/10), static_cast<char>('0'+error_%10)};
}

std::variant<StatusView, StatusError> validate_status(const Message& message) {
    if (message.command != "STA" || message.kind != Command::sta || message.parameters.size() < 2 ||
        message.parameters.size() > max_parameters) { return StatusError::invalid_message; }
    const auto code = StatusCode::parse(message.parameters[0]);
    if (!code) { return StatusError::invalid_code; }
    if (message.parameters[1].size() > max_status_text_bytes) { return StatusError::field_limit; }
    if (!std::holds_alternative<std::string>(serialize(message))) { return StatusError::invalid_message; }
    std::set<std::string_view> names;
    for (std::size_t i=2; i<message.parameters.size(); ++i) {
        const auto field = *parse_named_parameter(message.parameters[i]);
        if (!names.insert(field.name).second) { return StatusError::duplicate_field; }
        if (field.value.size() > 1024) { return StatusError::field_limit; }
        if (field.name == "FC" && !fourcc(field.value)) { return StatusError::invalid_field; }
        if ((field.name == "FM" || field.name == "FB") && !field_name(field.value)) { return StatusError::invalid_field; }
        if (field.name == "TL") {
            const auto time = parse_integer(field.value);
            if (!time || *time < 0) { return StatusError::invalid_field; }
        }
        if ((field.name == "TO" || field.name == "PR") && !text_atom(field.value)) { return StatusError::invalid_field; }
        if ((field.name == "I4" || field.name == "I6") && !ip(field.value, field.name == "I6")) { return StatusError::invalid_field; }
    }
    return StatusView{*code, message.parameters[1]};
}
std::variant<QuitView, StatusError> validate_quit(const Message& message) {
    if (message.command != "QUI" || message.kind != Command::qui || message.type != MessageType::info ||
        message.parameters.empty() || message.parameters.size() > max_parameters ||
        !std::holds_alternative<std::string>(serialize(message))) { return StatusError::invalid_message; }
    const auto sid = Sid::parse(message.parameters[0]);
    if (!sid) { return StatusError::invalid_field; }
    QuitView result{*sid, {}, {}, {}, {}, false};
    std::set<std::string_view> names;
    for (std::size_t i=1; i<message.parameters.size(); ++i) {
        const auto field = *parse_named_parameter(message.parameters[i]);
        if (!names.insert(field.name).second) { return StatusError::duplicate_field; }
        if (field.value.size() > max_status_text_bytes) { return StatusError::field_limit; }
        if (field.name == "ID") {
            result.initiator = Sid::parse(field.value);
            if (!result.initiator) { return StatusError::invalid_field; }
        } else if (field.name == "TL") {
            result.reconnect_after = parse_integer(field.value);
            if (!result.reconnect_after || *result.reconnect_after < -1) { return StatusError::invalid_field; }
        } else if (field.name == "MS") { result.message = field.value; }
        else if (field.name == "RD") {
            // Parsed metadata only. This module neither follows nor emits an
            // untrusted redirect. Future operator policy must validate its URL.
            if (field.value.empty()) { return StatusError::invalid_field; }
            result.redirect = field.value;
        } else if (field.name == "DI") { result.unwanted = true; }
    }
    return result;
}
Message public_status(const Message& message) {
    if (!std::holds_alternative<StatusView>(validate_status(message))) { throw std::invalid_argument("Invalid public status"); }
    Message result = message;
    result.parameters.resize(2);
    for (std::size_t i=2; i<message.parameters.size(); ++i) {
        if (known_status_field(parse_named_parameter(message.parameters[i])->name)) { result.parameters.push_back(message.parameters[i]); }
    }
    return result;
}
Message make_status(StatusCode code, std::string_view description, std::optional<std::string_view> command) {
    if (description.size() > max_status_text_bytes || (command && !fourcc(*command))) {
        throw std::invalid_argument("Invalid local status options");
    }
    Message result{MessageType::info, "STA", Command::sta, {}, {}, {}, {code.str(), std::string(description)}, 2};
    if (command) { result.parameters.emplace_back("FC"+std::string(*command)); }
    if (!std::holds_alternative<StatusView>(validate_status(result))) { throw std::invalid_argument("Invalid local status options"); }
    return result;
}
Message make_quit(Sid sid, QuitOptions options) {
    if (options.message.size() > max_status_text_bytes || options.redirect.size() > max_status_text_bytes ||
        (options.reconnect_after && *options.reconnect_after < -1)) { throw std::invalid_argument("Invalid local quit options"); }
    Message result{MessageType::info, "QUI", Command::qui, {}, {}, {}, {sid.str()}, 1};
    if (options.initiator) { result.parameters.push_back("ID"+options.initiator->str()); }
    if (options.reconnect_after) { result.parameters.push_back("TL"+std::to_string(*options.reconnect_after)); }
    if (!options.message.empty()) { result.parameters.push_back("MS"+std::string(options.message)); }
    if (!options.redirect.empty()) { result.parameters.push_back("RD"+std::string(options.redirect)); }
    if (options.unwanted) { result.parameters.emplace_back("DI"); }
    if (!std::holds_alternative<QuitView>(validate_quit(result))) { throw std::invalid_argument("Invalid local quit options"); }
    return result;
}
} // namespace dc24h::adc
