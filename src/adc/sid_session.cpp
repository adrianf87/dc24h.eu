#include "dc24h/adc/sid_session.hpp"

#include <stdexcept>
#include <utility>

namespace dc24h::adc {
std::string_view sid_status_code(SidError error) noexcept {
    return error == SidError::exhausted ? "211" : "240";
}

SidSession::SidSession(std::shared_ptr<SidPool> pool, SupportPolicy policy)
    : pool_(std::move(pool)), protocol_(std::move(policy)) {
    if (!pool_) { throw std::invalid_argument("SID session requires a hub pool"); }
}

std::optional<SidError> SidSession::prepare_sid() noexcept {
    if (protocol_.state() != ConnectionState::protocol) { return SidError::invalid_state; }
    if (!sup_sent_ || lease_) { return SidError::invalid_sequence; }
    auto candidate = pool_->acquire();
    if (!candidate) { return SidError::exhausted; }
    lease_ = std::move(candidate);
    return std::nullopt;
}

std::optional<Message> SidSession::sid_reply() const {
    const auto reserved = lease_.sid();
    if (!reserved) { return std::nullopt; }
    return Message{MessageType::info, "SID", Command::sid, {}, {}, {}, {reserved->str()}, 1};
}
std::optional<std::string> SidSession::sid_reply_line() const {
    const auto reserved = lease_.sid();
    if (!reserved) { return std::nullopt; }
    return "ISID " + reserved->str() + '\n';
}

SidSessionResult SidSession::observe(Direction direction, const Message& message) {
    if (protocol_.state() == ConnectionState::disconnected) { return SidSessionError{StateError::disconnected}; }
    if (direction == Direction::to_client && message.kind == Command::sid && message.command == "SID") {
        if (protocol_.state() != ConnectionState::protocol) { return SidSessionError{SidError::invalid_state}; }
        if (!lease_) { return SidSessionError{SidError::not_reserved}; }
        const auto expected = lease_.sid()->encode();
        if (message.type != MessageType::info || message.source || message.target || !message.features.empty() ||
            message.parameters.size() != 1 || message.parameters[0] != std::string_view(expected.data(), expected.size())) {
            return SidSessionError{SidError::reply_mismatch};
        }
    }
    if (const auto error = protocol_.observe(direction, message)) {
        return std::visit([](auto value) -> SidSessionError { return value; }, *error);
    }
    if (direction == Direction::to_client && message.kind == Command::sup) { sup_sent_ = true; }
    if (protocol_.state() == ConnectionState::disconnected) {
        lease_.reset();
        sup_sent_ = false;
    }
    return std::nullopt;
}

void SidSession::disconnect() noexcept {
    protocol_.disconnect();
    lease_.reset();
    sup_sent_ = false;
}
} // namespace dc24h::adc
