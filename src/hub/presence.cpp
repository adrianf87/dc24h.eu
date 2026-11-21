#include "dc24h/hub/presence.hpp"

#include "dc24h/adc/parser.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utf8proc.h>
#include <utility>

namespace dc24h::hub {
namespace {

using adc::MessageType;
constexpr std::size_t burst = 8;
constexpr auto token_interval = std::chrono::milliseconds(250);

const Rejection malformed_info{"40", "Invalid INF fields", {}};
const Rejection missing_nickname{"43", "Nickname is required", "FMNI"};
const Rejection invalid_nickname{"21", "Invalid nickname", {}};
const Rejection nickname_taken{"22", "Nickname is already in use", {}};
const Rejection invalid_cid{"43", "Invalid client identity", "FBID"};
const Rejection cid_taken{"24", "Client identity is already online", {}};
const Rejection invalid_message{"40", "Invalid chat message", {}};
const Rejection rate_limit{"40", "Update or chat rate limit exceeded", {}};

Rejection denied(const adc::Message &message, std::string_view description) {
    return {"25", description, "FC" + message.fourcc()};
}

template <typename Check> bool each_codepoint(std::string_view text, Check check) {
    while (!text.empty()) {
        utf8proc_int32_t point = 0;
        const auto used = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t *>(text.data()),
                                           static_cast<utf8proc_ssize_t>(text.size()), &point);
        if (used <= 0 || !check(point, utf8proc_category(point))) {
            return false;
        }
        text.remove_prefix(static_cast<std::size_t>(used));
    }
    return true;
}

std::optional<std::string> nickname_key(std::string_view text, std::size_t limit) {
    std::size_t count = 0;
    if (text.empty() || !each_codepoint(text, [&](auto point, auto category) {
            return ++count <= limit && point > 32 && category != UTF8PROC_CATEGORY_CC &&
                   category != UTF8PROC_CATEGORY_CF && category != UTF8PROC_CATEGORY_ZS &&
                   category != UTF8PROC_CATEGORY_ZL && category != UTF8PROC_CATEGORY_ZP;
        })) {
        return std::nullopt;
    }
    utf8proc_uint8_t *folded = nullptr;
    constexpr auto options =
        static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE | UTF8PROC_CASEFOLD);
    const auto size = utf8proc_map(reinterpret_cast<const utf8proc_uint8_t *>(text.data()),
                                   static_cast<utf8proc_ssize_t>(text.size()), &folded, options);
    const std::unique_ptr<utf8proc_uint8_t, decltype(&std::free)> key(folded, &std::free);
    if (size <= 0 || !key) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char *>(key.get()), static_cast<std::size_t>(size));
}

bool decimal(std::string_view value, std::uint64_t maximum) {
    std::uint64_t number = 0;
    const auto end = value.data() + value.size();
    const auto parsed = std::from_chars(value.data(), end, number);
    return parsed.ec == std::errc{} && parsed.ptr == end && number <= maximum;
}

bool allowed_field(std::string_view name) {
    constexpr std::string_view public_names[] = {"NI", "ID", "DE", "VE", "AP", "AW", "SU", "I4",
                                                 "I6", "U4", "U6", "SS", "SF", "SL", "HN", "HR",
                                                 "HO", "US", "DS", "AS", "AM", "KP"};
    return std::ranges::find(public_names, name) != std::end(public_names);
}

bool valid_field(const adc::Field &field) {
    if (field.value.empty() || field.name == "NI" || field.name == "SU") {
        return true;
    }
    if (field.name == "AW" || field.name == "U4" || field.name == "U6") {
        return decimal(field.value, field.name == "AW" ? 2U : 65535U);
    }
    if (field.name == "KP") {
        return adc::is_keyprint(field.value);
    }
    if (field.name == "DE" || field.name == "VE" || field.name == "AP" || field.name == "I4" ||
        field.name == "I6") {
        return each_codepoint(field.value, [](auto point, auto category) {
            return point >= 32 && category != UTF8PROC_CATEGORY_CC;
        });
    }
    return decimal(field.value, std::numeric_limits<std::uint64_t>::max());
}

bool feature_list(std::string_view value, std::set<std::string, std::less<>> &features) {
    features.clear();
    if (value.empty()) {
        return true;
    }
    while (true) {
        const auto separator = value.find(',');
        const auto name = value.substr(0, separator);
        if (!adc::is_feature(name) || !features.emplace(name).second || features.size() > 64) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        value.remove_prefix(separator + 1);
    }
}

template <typename Fields> WireFrame encode_info(std::string_view sid, const Fields &fields) {
    std::string line = "BINF ";
    line += sid;
    for (const auto &[name, value] : fields) {
        line += ' ';
        line += name;
        line += adc::escape_parameter(value);
    }
    line += '\n';
    return std::make_shared<const std::string>(std::move(line));
}

WireFrame encode_chat(const adc::Message &message, const adc::Chat &chat) {
    auto line = adc::format_routing_header(message);
    line += ' ';
    line += adc::escape_parameter(chat.text);
    if (chat.private_group) {
        line += " PM";
        line += *chat.private_group;
    }
    if (chat.third_person) {
        line += " ME1";
    }
    line += '\n';
    return std::make_shared<const std::string>(std::move(line));
}

} // namespace

WireFrame Rejection::reply(bool fatal) const {
    auto status = std::string(1, fatal ? '2' : '1');
    status += code;
    return std::make_shared<const std::string>(adc::format_status(status, description, field));
}

Presence::Presence(std::size_t nickname_limit, std::size_t message_limit, std::size_t profile_limit)
    : nickname_limit_(nickname_limit), message_limit_(message_limit),
      profile_limit_(profile_limit) {
    if (nickname_limit == 0 || nickname_limit > 256 || message_limit == 0 || message_limit > 4096) {
        throw std::invalid_argument("Nickname and chat limits must be within 1-256 and 1-4096");
    }
}

std::optional<Rejection> Presence::apply(Profile &profile, const adc::Message &message,
                                         std::string_view sid, bool initial) const {
    const auto fields = adc::parse_info(message);
    if (!fields) {
        return malformed_info;
    }
    for (const auto &field : *fields) {
        if (!allowed_field(field.name)) {
            continue;
        }
        if (field.name == "ID") {
            if (!adc::is_cid(field.value) || (!initial && profile.fields.at("ID") != field.value)) {
                return invalid_cid;
            }
        } else if (!valid_field(field) ||
                   (field.name == "SU" && !feature_list(field.value, profile.features))) {
            return Rejection{"43", "Invalid public INF field", "FB" + std::string(field.name)};
        }
        if (field.value.empty()) {
            if (const auto existing = profile.fields.find(field.name);
                existing != profile.fields.end()) {
                profile.fields.erase(existing);
            }
        } else {
            profile.fields.insert_or_assign(std::string(field.name), std::string(field.value));
        }
    }

    const auto nickname = profile.fields.find("NI");
    if (nickname == profile.fields.end()) {
        return initial ? missing_nickname : invalid_nickname;
    }
    if (!profile.fields.contains("ID")) {
        return Rejection{"43", "Client identity is required", "FMID"};
    }
    auto key = nickname_key(nickname->second, nickname_limit_);
    if (!key) {
        return invalid_nickname;
    }
    for (const auto &[other_sid, user] : users_) {
        if (other_sid == sid) {
            continue;
        }
        if (user.profile.nickname_key == *key) {
            return nickname_taken;
        }
        if (initial && user.profile.fields.at("ID") == profile.fields.at("ID")) {
            return cid_taken;
        }
    }
    profile.nickname_key = std::move(*key);
    profile.information = encode_info(sid, profile.fields);
    if (profile.information->size() - 1 > profile_limit_) {
        return Rejection{"40", "Public profile exceeds the input limit", {}};
    }
    return std::nullopt;
}

Presence::ProfileResult Presence::admit(std::string_view sid, const adc::Message &message,
                                        Clock::time_point now) {
    if (!adc::is_sid(sid) || users_.contains(sid) || message.sender_sid != sid ||
        message.type != MessageType::broadcast) {
        return denied(message, "Connection cannot claim this SID");
    }
    Profile profile;
    if (auto error = apply(profile, message, sid, true)) {
        return *error;
    }
    const auto information = profile.information;
    users_.emplace(std::string(sid), User{std::move(profile), {burst, token_interval, now}});
    return information;
}

Presence::ProfileResult Presence::update(std::string_view sid, const adc::Message &message,
                                         Clock::time_point now) {
    const auto found = users_.find(sid);
    if (found == users_.end() || message.sender_sid != sid ||
        message.type != MessageType::broadcast) {
        return denied(message, "Connection cannot update this profile");
    }
    auto &user = found->second;
    if (!user.commands.take(now)) {
        return rate_limit;
    }
    auto candidate = user.profile;
    if (auto error = apply(candidate, message, sid, false)) {
        return *error;
    }
    Fields changed;
    for (const auto &[key, value] : candidate.fields) {
        const auto before = user.profile.fields.find(key);
        if (before == user.profile.fields.end() || before->second != value) {
            changed.emplace(key, value);
        }
    }
    for (const auto &entry : user.profile.fields) {
        if (!candidate.fields.contains(entry.first)) {
            changed.emplace(entry.first, "");
        }
    }
    if (changed.empty()) {
        return WireFrame{};
    }
    auto line = encode_info(sid, changed);
    if (line->size() - 1 > profile_limit_) {
        return Rejection{"40", "Public INF update exceeds the input limit", {}};
    }
    user.profile = std::move(candidate);
    return line;
}

Presence::ChatResult Presence::chat(std::string_view sid, const adc::Message &message,
                                    Clock::time_point now) {
    const auto sender = users_.find(sid);
    if (sender == users_.end() || message.sender_sid != sid) {
        return denied(message, "Connection cannot send as this SID");
    }
    if (!sender->second.commands.take(now)) {
        return rate_limit;
    }
    const auto parsed = adc::parse_chat(message);
    if (!parsed || !each_codepoint(parsed->text, [](auto point, auto category) {
            return category != UTF8PROC_CATEGORY_CC || point == '\n' || point == '\t';
        })) {
        return invalid_message;
    }
    if (parsed->text.size() > message_limit_) {
        return Rejection{"40", "Chat message exceeds the text limit", {}};
    }

    const bool directed = message.type == MessageType::direct || message.type == MessageType::echo;
    if (directed) {
        if (!message.target_sid || !adc::is_sid(*message.target_sid) ||
            parsed->private_group != sid) {
            return invalid_message;
        }
    } else if (message.type == MessageType::broadcast || message.type == MessageType::feature) {
        if (parsed->private_group || message.target_sid ||
            (message.type == MessageType::feature && message.selectors.empty())) {
            return invalid_message;
        }
    } else {
        return invalid_message;
    }
    auto destinations = recipients(message);
    if (!destinations) {
        return denied(message, "Recipient is unavailable");
    }
    return ChatDelivery{encode_chat(message, *parsed), std::move(*destinations), directed};
}

WireFrame Presence::find(std::string_view sid) const {
    const auto user = users_.find(sid);
    return user == users_.end() ? WireFrame{} : user->second.profile.information;
}

std::optional<PeerCapabilities> Presence::capabilities(std::string_view sid) const {
    const auto user = users_.find(sid);
    if (user == users_.end()) {
        return std::nullopt;
    }
    const auto &[fields, nickname, features, information] = user->second.profile;
    const auto has_udp = [&](std::string_view ip, std::string_view port, std::string_view flag) {
        const auto advertised = fields.find(port);
        return fields.contains(ip) && features.contains(flag) && advertised != fields.end() &&
               advertised->second.find_first_not_of('0') != std::string::npos;
    };
    return PeerCapabilities{
        (fields.contains("I4") && features.contains("TCP4")) ||
            (fields.contains("I6") && features.contains("TCP6")),
        has_udp("I4", "U4", "UDP4") || has_udp("I6", "U6", "UDP6"),
        features.contains("ADCS"),
        features.contains("ADC0"),
    };
}

std::optional<std::vector<std::string_view>>
Presence::recipients(const adc::Message &message) const {
    std::vector<std::string_view> result;
    if (message.type == MessageType::direct || message.type == MessageType::echo) {
        if (!message.target_sid) {
            return std::nullopt;
        }
        const auto target = users_.find(*message.target_sid);
        if (target == users_.end()) {
            return std::nullopt;
        }
        result.push_back(target->first);
        if (message.type == MessageType::echo && message.sender_sid != message.target_sid &&
            message.sender_sid) {
            const auto sender = users_.find(*message.sender_sid);
            if (sender != users_.end()) {
                result.push_back(sender->first);
            }
        }
    } else if (message.type == MessageType::broadcast || message.type == MessageType::feature) {
        result.reserve(users_.size());
        for (const auto &[sid, user] : users_) {
            if (message.type == MessageType::broadcast ||
                std::ranges::all_of(message.selectors, [&](const adc::FeatureSelector &selector) {
                    return user.profile.features.contains(selector.name) == selector.required;
                })) {
                result.push_back(sid);
            }
        }
    } else {
        return std::nullopt;
    }
    return result;
}

std::vector<RosterEntry> Presence::roster() const {
    std::vector<RosterEntry> list;
    list.reserve(users_.size());
    for (const auto &[sid, user] : users_) {
        list.push_back({sid, user.profile.information});
    }
    return list;
}

std::vector<OnlineUser> Presence::user_page(std::string_view after, std::size_t limit) const {
    std::vector<OnlineUser> page;
    for (auto user = users_.upper_bound(after); user != users_.end() && page.size() < limit;
         ++user) {
        const auto &fields = user->second.profile.fields;
        page.push_back({user->first, fields.at("NI"), fields.at("ID")});
    }
    return page;
}

std::size_t Presence::size() const noexcept { return users_.size(); }

bool Presence::remove(std::string_view sid) {
    const auto user = users_.find(sid);
    if (user == users_.end()) {
        return false;
    }
    users_.erase(user);
    return true;
}

} // namespace dc24h::hub
