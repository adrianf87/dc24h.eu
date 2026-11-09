#include "dc24h/adc/inf.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <arpa/inet.h>
#include <openssl/crypto.h>
#include <utf8proc.h>

#include <algorithm>
#include <bitset>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dc24h::adc {
namespace {
bool numeric_field(std::string_view name) noexcept {
    constexpr std::string_view names[]{"U4", "U6", "SS", "SF", "US", "DS", "SL", "AS", "AM", "HN", "HR", "HO", "CT", "AW", "FS"};
    return std::find(std::begin(names), std::end(names), name) != std::end(names);
}
std::size_t text_limit(std::string_view name) noexcept {
    if (name == "NI") { return max_nick_bytes; }
    if (name == "DE" || name == "RF") { return 2048; }
    if (name == "EM") { return 254; }
    if (name == "AP" || name == "VE") { return 128; }
    if (name == "LO") { return 64; }
    return 0;
}
bool valid_text(std::string_view text, bool nickname) {
    utf8proc_uint8_t* raw = nullptr;
    const auto length = utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(text.data()),
        static_cast<utf8proc_ssize_t>(text.size()), &raw,
        static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE));
    const std::unique_ptr<utf8proc_uint8_t, decltype(&std::free)> normalized(raw, &std::free);
    if (length == UTF8PROC_ERROR_NOMEM) { throw std::bad_alloc(); }
    if (length < 0 || static_cast<std::size_t>(length) != text.size() ||
        !std::equal(text.begin(), text.end(), reinterpret_cast<const char*>(raw))) { return false; }
    std::size_t offset = 0;
    while (offset < text.size()) {
        utf8proc_int32_t codepoint = 0;
        const auto count = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(text.data()+offset),
            static_cast<utf8proc_ssize_t>(text.size()-offset), &codepoint);
        if (count <= 0 || codepoint < (nickname ? 33 : 32)) { return false; }
        const auto category = utf8proc_category(codepoint);
        if (category == UTF8PROC_CATEGORY_CC || category == UTF8PROC_CATEGORY_CF ||
            category == UTF8PROC_CATEGORY_ZL || category == UTF8PROC_CATEGORY_ZP) { return false; }
        offset += static_cast<std::size_t>(count);
    }
    return true;
}
bool valid_features(std::string_view text) {
    std::set<std::string_view> seen;
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto name = text.substr(0, comma);
        if (name.size() != 4 || name[0] < 'A' || name[0] > 'Z' ||
            !std::all_of(name.begin()+1, name.end(), [](char c) {
                return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
            }) || !seen.insert(name).second || seen.size() > max_support_features) { return false; }
        if (comma == std::string_view::npos) { return true; }
        text.remove_prefix(comma+1);
        if (text.empty()) { return false; }
    }
    return false;
}
struct PrivateDigest {
    std::optional<TigerDigest> value;
    ~PrivateDigest() { if (value) { OPENSSL_cleanse(value->data(), value->size()); } }
};
} // namespace

std::string_view inf_status_code(InfError error) noexcept {
    switch (error) {
    case InfError::invalid_pid: case InfError::cid_mismatch: return "227";
    case InfError::invalid_nick: return "221";
    case InfError::invalid_ip: return "246";
    case InfError::unsupported_hash: return "247";
    case InfError::crypto_unavailable: return "210";
    default: return "243";
    }
}
std::optional<std::string_view> ClientInfo::field(std::string_view name) const noexcept {
    const auto found = fields_.find(name);
    if (found == fields_.end()) { return std::nullopt; }
    return found->second;
}
std::optional<std::int64_t> ClientInfo::integer(std::string_view name) const noexcept {
    if (!numeric_field(name)) { return std::nullopt; }
    const auto value = field(name);
    return value ? parse_integer(*value) : std::nullopt;
}
std::size_t ClientInfo::bytes() const noexcept {
    std::size_t count = 0;
    for (const auto& [name, value] : fields_) { count += name.size()+value.size(); }
    return count;
}
Message ClientInfo::public_message(Sid source) const {
    Message result{MessageType::broadcast, "INF", Command::inf, source.str(), {}, {}, {}, 0};
    for (const auto& [name, value] : fields_) { result.parameters.push_back(name+value); }
    return result;
}
InfValidator::InfValidator(std::string_view peer_address) : family_(AF_INET) {
    if (peer_address.size() > INET6_ADDRSTRLEN || peer_address.find('\0') != std::string_view::npos) {
        throw std::invalid_argument("Invalid numeric INF peer address");
    }
    const std::string text(peer_address);
    if (inet_pton(AF_INET, text.c_str(), address_.data()) == 1) { return; }
    family_ = AF_INET6;
    if (inet_pton(AF_INET6, text.c_str(), address_.data()) != 1) {
        throw std::invalid_argument("Invalid numeric INF peer address");
    }
    const std::array<unsigned char, 12> mapped{0,0,0,0,0,0,0,0,0,0,255,255};
    if (std::equal(mapped.begin(), mapped.end(), address_.begin())) {
        std::copy_n(address_.begin()+12, 4, address_.begin());
        family_ = AF_INET;
    }
}

std::variant<ClientInfo, InfError> InfValidator::validate(const Message& message, Sid source,
    SessionHash hash, const ClientInfo* previous) const {
    if (message.command != "INF" || message.kind != Command::inf || message.type != MessageType::broadcast ||
        message.target || !message.features.empty() || message.parameters.size() > max_parameters) {
        return InfError::invalid_message;
    }
    if (!message.source || Sid::parse(*message.source) != source) { return InfError::wrong_source; }
    if (hash != SessionHash::tiger192) { return InfError::unsupported_hash; }
    auto candidate = previous ? *previous : ClientInfo{};
    std::bitset<26*36> seen;
    std::string_view pid;
    for (const auto& token : message.parameters) {
        if (token.size() > max_frame_bytes) { return InfError::field_limit; }
        const auto parameter = parse_named_parameter(token);
        if (!parameter) { return InfError::invalid_message; }
        const auto [name, value] = *parameter;
        const auto second = name[1] >= 'A' ? name[1]-'A' : name[1]-'0'+26;
        const auto index = static_cast<std::size_t>((name[0]-'A')*36+second);
        if (seen.test(index)) { return InfError::duplicate_field; }
        seen.set(index);
        if (name == "PD") {
            if (previous) { return InfError::unexpected_pid; }
            pid = value; // Borrow only; never insert into public metadata.
            continue;
        }
        if (name == "ID") {
            if (!parse_tiger_base32(value)) { return InfError::invalid_cid; }
            if (previous && previous->field("ID") != value) { return InfError::immutable_identity; }
        } else if (numeric_field(name)) {
            if (!value.empty()) {
                const auto number = parse_integer(value);
                if (!number || *number < 0 || ((name == "U4" || name == "U6") && *number > 65535) ||
                    (name == "AW" && *number > 2)) { return InfError::invalid_integer; }
                if (name == "CT" && *number > 1) { return InfError::privilege_claim; }
            }
        } else if (name == "I4" || name == "I6") {
            if (!value.empty()) {
                const int family = name == "I4" ? AF_INET : AF_INET6;
                std::array<unsigned char, 16> supplied{};
                if (family != family_ || value.size() >= INET6_ADDRSTRLEN || value.find('\0') != std::string_view::npos ||
                    inet_pton(family, std::string(value).c_str(), supplied.data()) != 1) { return InfError::invalid_ip; }
                const auto length = family == AF_INET ? 4U : 16U;
                const bool unspecified = std::all_of(supplied.begin(), supplied.begin()+length,
                                                    [](auto byte) { return byte == 0; });
                if (!unspecified && !std::equal(supplied.begin(), supplied.begin()+length, address_.begin())) {
                    return InfError::invalid_ip;
                }
                char normalized[INET6_ADDRSTRLEN]{};
                if (!inet_ntop(family, address_.data(), normalized, sizeof(normalized))) { return InfError::invalid_ip; }
                candidate.fields_[std::string(name)] = normalized;
                if (candidate.bytes() > max_info_bytes) { return InfError::field_limit; }
                continue;
            }
        } else if (name == "SU") {
            if (!value.empty() && (value.size() > max_support_features*5-1 || !valid_features(value))) {
                return InfError::invalid_features;
            }
        } else {
            const auto limit = text_limit(name);
            if (limit == 0) { continue; } // Unknown fields are ignored, never forwarded.
            if (value.size() > limit) { return InfError::field_limit; }
            if (!value.empty() && !valid_text(value, name == "NI")) {
                return name == "NI" ? InfError::invalid_nick : InfError::invalid_text;
            }
        }
        if (value.empty()) { candidate.fields_.erase(std::string(name)); }
        else { candidate.fields_[std::string(name)] = value; }
        if (candidate.bytes() > max_info_bytes) { return InfError::field_limit; }
    }
    if (!candidate.field("ID") || !candidate.field("NI")) { return InfError::missing_identity; }
    if (!previous) {
        if (pid.empty()) { return InfError::missing_identity; }
        PrivateDigest decoded{parse_tiger_base32(pid)};
        if (!decoded.value) { return InfError::invalid_pid; }
        const auto digest = tiger192(*decoded.value);
        if (!digest) { return InfError::crypto_unavailable; }
        if (tiger_base32(*digest) != *candidate.field("ID")) { return InfError::cid_mismatch; }
    }
    return candidate;
}

ClientSession::ClientSession(std::shared_ptr<SidPool> pool, std::string_view peer_address, SupportPolicy policy)
    : session_(std::move(pool), std::move(policy)), validator_(peer_address) {}

ClientSessionResult ClientSession::observe(Direction direction, const Message& message) {
    static_assert(std::is_nothrow_move_assignable_v<std::optional<ClientInfo>>);
    if (session_.state() == ConnectionState::disconnected) { return ClientSessionError{StateError::disconnected}; }
    if (message.kind == Command::inf && message.command == "INF") {
        if (direction == Direction::from_client) {
            const auto phase = state();
            if (phase != ConnectionState::identify && phase != ConnectionState::normal) {
                return ClientSessionError{StateError::invalid_state};
            }
            const auto candidate = validator_.validate(message, *Sid::parse(session_.sid()),
                                                       *support().selected_hash(), info_ ? &*info_ : nullptr);
            if (const auto* error = std::get_if<InfError>(&candidate)) { return ClientSessionError{*error}; }
            // Copy before state mutation; memory failure leaves both unchanged.
            std::optional<ClientInfo> owned{std::get<ClientInfo>(candidate)};
            if (const auto error = session_.observe(direction, message)) {
                return std::visit([](auto value) -> ClientSessionError { return value; }, *error);
            }
            info_ = std::move(owned);
            return std::nullopt;
        }
        for (const auto& token : message.parameters) {
            const auto field = parse_named_parameter(token);
            if (field && field->name == "PD") { return ClientSessionError{InfError::unexpected_pid}; }
        }
    }
    if (const auto error = session_.observe(direction, message)) {
        return std::visit([](auto value) -> ClientSessionError { return value; }, *error);
    }
    if (session_.state() == ConnectionState::disconnected) { info_.reset(); }
    return std::nullopt;
}
ClientSessionResult ClientSession::approve_identity(IdentityMode mode) noexcept {
    if (session_.state() == ConnectionState::disconnected) { return ClientSessionError{StateError::disconnected}; }
    if (!info_) { return ClientSessionError{InfError::missing_identity}; }
    if (const auto error = session_.approve_identity(mode)) { return ClientSessionError{*error}; }
    return std::nullopt;
}
void ClientSession::disconnect() noexcept { session_.disconnect(); info_.reset(); }
} // namespace dc24h::adc
