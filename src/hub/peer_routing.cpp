#include "dc24h/hub/peer_routing.hpp"

#include <algorithm>

namespace dc24h::hub {
namespace {

using namespace std::chrono_literals;
using adc::MessageType;

constexpr auto request_lifetime = 60s;
constexpr std::size_t search_burst = 8;
constexpr std::size_t searches_per_client = 16;
constexpr std::size_t max_searches = 1024;
constexpr std::size_t max_responders = 65536;
constexpr std::size_t max_reverse_requests = 1024;

Rejection reject(const adc::Message &message, std::string_view description,
                 std::string_view code = "40") {
    return {code, description, "FC" + message.fourcc()};
}

Rejection unavailable(const adc::Message &message) {
    return reject(message, "Recipient is unavailable", "25");
}

bool directed(const adc::Message &message) {
    return message.type == MessageType::direct || message.type == MessageType::echo;
}

bool supports_protocol(std::string_view protocol, PeerCapabilities first, PeerCapabilities second) {
    if (protocol == "ADC/1.0") {
        return true;
    }
    if (protocol == "ADCS/1.0") {
        return first.adcs && second.adcs;
    }
    // ADC0 clients use the legacy protocol name for the same encrypted connection.
    return protocol == "ADCS/0.10" && (first.adcs || first.legacy_adcs) &&
           (second.adcs || second.legacy_adcs);
}

} // namespace

PeerRouting::Limits::Limits(Clock::time_point now)
    : searches(search_burst, 1s, now), results(160, 10ms, now), connections(8, 250ms, now) {}

PeerRouting::PeerRouting(const Presence &presence, Send send)
    : presence_(presence), send_(std::move(send)) {}

std::optional<Rejection> PeerRouting::route(std::string_view sid, const adc::Message &message,
                                            Clock::time_point now) {
    const auto source = presence_.capabilities(sid);
    if (!source || message.sender_sid != sid) {
        return reject(message, "Connection cannot send as this SID", "25");
    }
    if (!adc::is_peer_command(message.command)) {
        return reject(message, "Unsupported peer command");
    }
    auto &limits = limits_.try_emplace(std::string(sid), now).first->second;
    expire(now);
    if (message.command == "SCH") {
        if (!limits.searches.take(now)) {
            return reject(message, "Search rate limit exceeded");
        }
        return search(sid, message, *source, now);
    }
    if (!directed(message) || !message.target_sid || *message.target_sid == sid) {
        return reject(message, "Command requires another client's SID");
    }
    if (message.command == "RES") {
        if (!limits.results.take(now)) {
            return reject(message, "Search result rate limit exceeded");
        }
        return result(sid, message, now);
    }
    if (!limits.connections.take(now)) {
        return reject(message, "Connection signaling rate limit exceeded");
    }
    return connect(sid, message, *source, now);
}

std::optional<Rejection> PeerRouting::search(std::string_view sid, const adc::Message &message,
                                             PeerCapabilities source, Clock::time_point now) {
    const auto token = adc::parse_search(message);
    if (!token) {
        return reject(message, "Invalid search parameters");
    }
    const auto recipients = presence_.recipients(message);
    if (!recipients) {
        return unavailable(message);
    }

    SearchKey key{std::string(sid), std::string(*token)};
    auto query = searches_.find(key);
    const auto additional = std::ranges::count_if(*recipients, [&](std::string_view recipient) {
        return recipient != sid && (query == searches_.end() || !query->second.contains(recipient));
    });
    if (static_cast<std::size_t>(additional) > max_responders - responder_count_) {
        return reject(message, "Too many pending searches");
    }
    if (additional != 0 && query == searches_.end()) {
        auto first = searches_.lower_bound({key.first, {}});
        std::size_t pending = 0;
        while (first != searches_.end() && first->first.first == sid) {
            ++pending;
            ++first;
        }
        if (searches_.size() >= max_searches || pending >= searches_per_client) {
            return reject(message, "Too many pending searches");
        }
        query = searches_.try_emplace(std::move(key)).first;
    }

    const auto frame = std::make_shared<const std::string>(adc::format_routed(message));
    const std::size_t credits = source.udp ? 10 : 5;
    bool target_unavailable = false;
    for (const auto recipient : *recipients) {
        if (!send_(recipient, frame)) {
            if (directed(message)) {
                target_unavailable = true;
                break;
            }
        } else if (recipient != sid) {
            auto [responder, inserted] = query->second.try_emplace(std::string(recipient));
            responder_count_ += inserted;
            auto &state = responder->second;
            if (state.deadline <= now) {
                state.remaining = 0;
            }
            state.deadline = now + request_lifetime;
            // Multiple client searches can share a token, including the omitted token.
            state.remaining = std::min(credits * search_burst, state.remaining + credits);
        }
    }
    if (query != searches_.end() && query->second.empty()) {
        searches_.erase(query);
    }
    return target_unavailable ? std::optional{unavailable(message)} : std::nullopt;
}

std::optional<Rejection> PeerRouting::result(std::string_view sid, const adc::Message &message,
                                             Clock::time_point now) {
    const auto token = adc::parse_search_result(message);
    if (!token) {
        return reject(message, "Invalid search result parameters");
    }
    if (!presence_.capabilities(*message.target_sid)) {
        return unavailable(message);
    }
    const auto query = searches_.find({*message.target_sid, std::string(*token)});
    if (query == searches_.end()) {
        return reject(message, "Unknown or expired search token");
    }
    const auto responder = query->second.find(sid);
    if (responder == query->second.end() || responder->second.deadline <= now) {
        return reject(message, "Unknown or expired search token");
    }
    if (responder->second.remaining == 0) {
        return reject(message, "Search result limit reached");
    }
    if (!send_directed(message)) {
        return unavailable(message);
    }
    --responder->second.remaining;
    return std::nullopt;
}

std::optional<Rejection> PeerRouting::connect(std::string_view sid, const adc::Message &message,
                                              PeerCapabilities source, Clock::time_point now) {
    const auto request = adc::parse_peer_connection(message);
    if (!request) {
        return reject(message, "Invalid connection signaling parameters");
    }
    const auto &target_sid = *message.target_sid;
    const auto target = presence_.capabilities(target_sid);
    if (!target) {
        return unavailable(message);
    }
    if (!supports_protocol(request->protocol, source, *target)) {
        return reject(message, "Transfer protocol is unsupported by the peers", "25");
    }

    const bool ctm = request->port.has_value();
    if (ctm ? !source.tcp : !target->tcp) {
        return reject(message, "Connection requires a TCP-active peer", "25");
    }

    ReverseRequest key{ctm ? std::string(sid) : target_sid, ctm ? target_sid : std::string(sid),
                       std::string(request->protocol), std::string(request->token)};
    if (!ctm) {
        if (reverse_.size() >= max_reverse_requests && !reverse_.contains(key)) {
            return reject(message, "Too many pending reverse connections");
        }
        if (!send_directed(message)) {
            return unavailable(message);
        }
        reverse_.insert_or_assign(std::move(key), now + request_lifetime);
        return std::nullopt;
    }

    const auto response = reverse_.find(key);
    const bool matches = response != reverse_.end() && response->second > now;
    if (!matches) {
        auto pending = reverse_.lower_bound({key.active, key.requester, {}, {}});
        while (pending != reverse_.end() && pending->first.active == key.active &&
               pending->first.requester == key.requester) {
            if (pending->second > now) {
                return reject(message, "Reverse connection protocol or token does not match");
            }
            ++pending;
        }
    }
    if (!send_directed(message)) {
        return unavailable(message);
    }
    if (matches) {
        reverse_.erase(response);
    }
    return std::nullopt;
}

bool PeerRouting::send_directed(const adc::Message &message) {
    const auto frame = std::make_shared<const std::string>(adc::format_routed(message));
    if (!send_(*message.target_sid, frame)) {
        return false;
    }
    if (message.type == MessageType::echo) {
        static_cast<void>(send_(*message.sender_sid, frame));
    }
    return true;
}

void PeerRouting::expire(Clock::time_point now) {
    if (now < next_expiry_) {
        return;
    }
    next_expiry_ = now + 1s;
    for (auto query = searches_.begin(); query != searches_.end();) {
        auto &responders = query->second;
        responder_count_ -= std::erase_if(
            responders, [now](const auto &responder) { return responder.second.deadline <= now; });
        if (responders.empty()) {
            query = searches_.erase(query);
        } else {
            ++query;
        }
    }
    std::erase_if(reverse_, [now](const auto &request) { return request.second <= now; });
}

void PeerRouting::forget(std::string_view sid) {
    const std::string removed(sid);
    limits_.erase(removed);
    for (auto query = searches_.begin(); query != searches_.end();) {
        auto &responders = query->second;
        if (query->first.first == sid) {
            responder_count_ -= responders.size();
            query = searches_.erase(query);
        } else {
            responder_count_ -= responders.erase(removed);
            if (responders.empty()) {
                query = searches_.erase(query);
            } else {
                ++query;
            }
        }
    }
    std::erase_if(reverse_, [sid](const auto &request) {
        return request.first.active == sid || request.first.requester == sid;
    });
}

} // namespace dc24h::hub
