#include "dc24h/adc/sup.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dc24h::adc {
namespace {
bool feature_name(std::string_view name) noexcept {
    return name.size() == 4 && name[0] >= 'A' && name[0] <= 'Z' &&
        std::all_of(name.begin() + 1, name.end(), [](char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        });
}
bool contains(const std::vector<std::string>& features, std::string_view name) noexcept {
    const auto found = std::lower_bound(features.begin(), features.end(), name);
    return found != features.end() && *found == name;
}
std::optional<SupportError> apply(std::vector<std::string>& candidate, const Message& message) {
    // Recheck this small security boundary even for hand-built Message objects.
    if (message.type != MessageType::hub || message.kind != Command::sup || message.command != "SUP" ||
        message.source || message.target || !message.features.empty() || message.parameters.empty() ||
        message.parameters.size() > max_parameters) { return SupportError::invalid_message; }
    for (const auto& token : message.parameters) {
        if (token.size() != 6 || (token.compare(0, 2, "AD") != 0 && token.compare(0, 2, "RM") != 0) ||
            !feature_name(std::string_view(token).substr(2))) { return SupportError::invalid_message; }
        const auto feature = std::string_view(token).substr(2);
        auto position = std::lower_bound(candidate.begin(), candidate.end(), feature);
        const bool present = position != candidate.end() && *position == feature;
        if (token[0] == 'A' && !present) {
            if (candidate.size() == max_support_features) { return SupportError::feature_limit; }
            candidate.insert(position, std::string(feature));
        } else if (token[0] == 'R' && present) { candidate.erase(position); }
    }
    return std::nullopt;
}
} // namespace

std::string_view support_status_code(SupportError error) noexcept {
    switch (error) {
    case SupportError::missing_base: case SupportError::session_hash_removed: return "245";
    case SupportError::no_common_hash: return "247";
    default: return "240";
    }
}

SupportNegotiation::SupportNegotiation(SupportPolicy policy) : local_(std::move(policy.features)) {
    if (local_.empty() || local_.size() > max_support_features ||
        !std::all_of(local_.begin(), local_.end(), feature_name)) {
        throw std::invalid_argument("Invalid local SUP feature policy");
    }
    std::sort(local_.begin(), local_.end());
    local_.erase(std::unique(local_.begin(), local_.end()), local_.end());
    if (!contains(local_, "BASE") || !contains(local_, "TIGR")) {
        throw std::invalid_argument("Local SUP policy requires BASE and TIGR");
    }
}

std::optional<SupportError> SupportNegotiation::accept_initial(const Message& sup) {
    if (hash_) { return SupportError::invalid_sequence; }
    std::vector<std::string> candidate;
    if (const auto error = apply(candidate, sup)) { return error; }
    if (!contains(candidate, "BASE")) { return SupportError::missing_base; }
    // Unknown features are retained, but never inferred to be hash algorithms.
    if (!contains(candidate, "TIGR")) { return SupportError::no_common_hash; }
    peer_.swap(candidate);
    hash_ = SessionHash::tiger192;
    return std::nullopt;
}

std::optional<SupportError> SupportNegotiation::update_peer(const Message& sup) {
    if (!hash_) { return SupportError::invalid_sequence; }
    auto candidate = peer_;
    if (const auto error = apply(candidate, sup)) { return error; }
    if (!contains(candidate, "BASE")) { return SupportError::missing_base; }
    if (!contains(candidate, hash_feature(*hash_))) { return SupportError::session_hash_removed; }
    peer_.swap(candidate); // Session hash never changes during a connection.
    return std::nullopt;
}

bool SupportNegotiation::peer_supports(std::string_view feature) const noexcept { return contains(peer_, feature); }

std::optional<Message> SupportNegotiation::initial_reply() const {
    if (!hash_) { return std::nullopt; }
    Message reply{MessageType::info, "SUP", Command::sup, {}, {}, {}, {}, {}};
    reply.parameters.reserve(local_.size());
    reply.parameters.emplace_back("ADBASE");
    reply.parameters.emplace_back("AD" + std::string(hash_feature(*hash_)));
    for (const auto& feature : local_) {
        if (feature != "BASE" && feature != hash_feature(*hash_)) { reply.parameters.push_back("AD" + feature); }
    }
    reply.positional_count = reply.parameters.size();
    return reply;
}

std::optional<std::string> SupportNegotiation::initial_reply_line() const {
    const auto reply = initial_reply();
    if (!reply) { return std::nullopt; }
    std::string line = "ISUP";
    for (const auto& token : reply->parameters) { line += ' '; line += token; }
    line += '\n';
    return line;
}

ProtocolConnection::ProtocolConnection(SupportPolicy policy) : support_(std::move(policy)) {}

ProtocolResult ProtocolConnection::observe(Direction direction, const Message& message) {
    static_assert(std::is_nothrow_move_assignable_v<SupportNegotiation>);
    if (state_.state() == ConnectionState::disconnected) { return ProtocolError{StateError::disconnected}; }
    if (message.kind != Command::sup || message.command != "SUP") {
        if (const auto error = state_.observe(direction, message)) { return ProtocolError{*error}; }
        return std::nullopt;
    }
    const auto phase = state_.state();
    if (phase != ConnectionState::protocol && phase != ConnectionState::normal) {
        return ProtocolError{StateError::invalid_state};
    }
    if (direction == Direction::from_client) {
        auto candidate = support_;
        const auto support_error = phase == ConnectionState::protocol ? candidate.accept_initial(message) :
                                                                      candidate.update_peer(message);
        if (support_error) { return ProtocolError{*support_error}; }
        if (const auto error = state_.observe(direction, message)) { return ProtocolError{*error}; }
        if (phase == ConnectionState::protocol) {
            // The owned machine has just accepted its first HSUP; approval is
            // guaranteed here. No semantic validation is deferred to the caller.
            const auto error = state_.approve_protocol();
            assert(!error);
            (void)error;
        }
        support_ = std::move(candidate);
    } else {
        if (phase == ConnectionState::normal) { return ProtocolError{SupportError::local_update_unsupported}; }
        const auto reply = support_.initial_reply();
        if (!reply || message.type != reply->type || message.source || message.target ||
            !message.features.empty() || message.parameters != reply->parameters) {
            return ProtocolError{SupportError::reply_mismatch};
        }
        // State advances only when the owner reports a completed ordered send.
        if (const auto error = state_.observe(direction, message)) { return ProtocolError{*error}; }
    }
    return std::nullopt;
}

} // namespace dc24h::adc
