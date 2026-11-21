#include "dc24h/adc/session.hpp"

#include "dc24h/adc/parser.hpp"
#include "wire.hpp"

#include <stdexcept>
#include <utility>

namespace dc24h::adc {
namespace {

constexpr std::size_t max_peer_features = 64;

bool routed(MessageType type) {
    return type == MessageType::broadcast || type == MessageType::direct ||
           type == MessageType::echo || type == MessageType::feature;
}

std::string status_line(std::string_view code, std::string_view text, std::string_view field) {
    std::string line = "ISTA ";
    line += code;
    line += ' ';
    line += escape_parameter(text);
    if (!field.empty()) {
        line += ' ';
        line += escape_parameter(field);
    }
    line += '\n';
    return line;
}

SessionResult status(std::string_view code, std::string_view text, std::string_view field = {}) {
    return {SessionAction::none, {status_line(code, text, field)}};
}

SessionResult invalid_state(const Message &message) {
    return status("144", "Command is invalid in this state", "FC" + message.fourcc());
}

SessionResult invalid_direction(const Message &message) {
    return status("125", "Command is not allowed in this direction", "FC" + message.fourcc());
}

std::optional<std::string_view> field_value(const std::vector<Field> &fields,
                                            std::string_view name) {
    const auto found = std::ranges::find(fields, name, &Field::name);
    if (found != fields.end()) {
        return found->value;
    }
    return std::nullopt;
}

} // namespace

Session::Session(std::string_view assigned_sid) {
    if (!is_sid(assigned_sid)) {
        throw std::invalid_argument("The assigned ADC SID is invalid");
    }
    sid_ = assigned_sid;
}

SessionState Session::state() const noexcept { return state_; }

bool Session::closed() const noexcept { return closed_; }

std::string_view Session::cid() const noexcept { return cid_; }

bool Session::peer_supports(std::string_view feature) const {
    return peer_features_.contains(feature);
}

SessionResult Session::receive(const Message &message) {
    if (closed_) {
        return {SessionAction::close, {}};
    }
    if (message.command == "STA") {
        return receive_status(message);
    }
    if (message.type != MessageType::hub && !routed(message.type)) {
        return invalid_direction(message);
    }
    if (routed(message.type) && message.sender_sid != sid_) {
        return status("125", "Sender SID does not match the connection", "FC" + message.fourcc());
    }
    if (message.command == "SUP") {
        return receive_support(message);
    }
    if (message.command == "INF") {
        return receive_info(message);
    }
    if (message.command == "PAS") {
        return receive_password(message);
    }
    if (message.command == "SID" || message.command == "GPA" || message.command == "QUI") {
        return invalid_direction(message);
    }
    if (message.command == "GET" || message.command == "GFI" || message.command == "SND") {
        return invalid_direction(message);
    }
    if (state_ != SessionState::normal) {
        if (message.command == "MSG" || message.command == "SCH" || message.command == "RES" ||
            message.command == "CTM" || message.command == "RCM") {
            return invalid_state(message);
        }
        return {};
    }
    if (routed(message.type)) {
        return {SessionAction::dispatch, {}};
    }
    return {};
}

SessionResult Session::receive_support(const Message &message) {
    if (message.type != MessageType::hub) {
        return invalid_direction(message);
    }
    const bool initial = state_ == SessionState::protocol;
    if (!initial && state_ != SessionState::normal) {
        return invalid_state(message);
    }
    const auto changes = parse_support(message);
    if (!changes) {
        return status("140", "Invalid SUP parameters");
    }

    auto advertised = peer_features_;
    for (const auto &change : *changes) {
        if (change.added) {
            advertised.emplace(change.name);
            if (advertised.size() > max_peer_features) {
                return status("140", "Too many advertised features");
            }
        } else if (const auto existing = advertised.find(change.name);
                   existing != advertised.end()) {
            advertised.erase(existing);
        }
    }

    if (!advertised.contains("BASE")) {
        if (initial) {
            return fatal_status("245", "BASE support is required", "FCBASE");
        }
        return status("145", "BASE support is required", "FCBASE");
    }
    if (initial && !advertised.contains("TIGR")) {
        return fatal_status("247", "No common session hash");
    }
    peer_features_ = std::move(advertised);
    if (initial) {
        state_ = SessionState::identify;
        return {SessionAction::none, {"ISUP ADTIGR ADBASE\n", "ISID " + sid_ + '\n'}};
    }
    return {};
}

SessionResult Session::receive_info(const Message &message) {
    if (message.type != MessageType::broadcast) {
        return invalid_direction(message);
    }
    if (state_ != SessionState::normal && (state_ != SessionState::identify || !cid_.empty())) {
        return invalid_state(message);
    }
    const auto fields = parse_info(message);
    if (!fields) {
        return status("140", "Invalid INF fields");
    }
    const auto cid = field_value(*fields, "ID");
    const auto pid = field_value(*fields, "PD");
    if (state_ == SessionState::normal) {
        if (pid) {
            return status("143", "PID is only accepted during identification", "FBPD");
        }
        if (cid && *cid != cid_) {
            return status("143", "CID cannot change during a session", "FBID");
        }
        return {SessionAction::dispatch, {}};
    }

    if (!cid || cid->empty()) {
        return status("143", "CID is required", "FMID");
    }
    if (!pid || pid->empty()) {
        return status("143", "PID is required", "FMPD");
    }
    switch (verify_tiger_identity(*cid, *pid)) {
    case IdentityCheck::valid:
        cid_ = *cid;
        return {SessionAction::identified, {}};
    case IdentityCheck::invalid_cid:
        return status("143", "Invalid CID", "FBID");
    case IdentityCheck::invalid_pid:
    case IdentityCheck::mismatch:
        return status("127", "Invalid PID");
    case IdentityCheck::hash_unavailable:
        return fatal_status("210", "Session hash is unavailable");
    }
    return {};
}

SessionResult Session::receive_password(const Message &message) {
    if (message.type != MessageType::hub) {
        return invalid_direction(message);
    }
    if (state_ != SessionState::verify || password_pending_) {
        return invalid_state(message);
    }
    if (!parse_password_response(message)) {
        return status("140", "Invalid PAS parameter");
    }
    password_pending_ = true;
    return {SessionAction::verify_password, {}};
}

SessionResult Session::receive_status(const Message &message) {
    const auto received = parse_status(message);
    if (!received) {
        return {};
    }
    if (message.type == MessageType::hub && received->severity == Severity::fatal) {
        closed_ = true;
        return {SessionAction::close, {}};
    }
    if (state_ == SessionState::normal && routed(message.type) && message.sender_sid == sid_) {
        return {SessionAction::dispatch, {}};
    }
    return {};
}

std::optional<SessionResult>
Session::complete_identification(std::optional<std::string_view> challenge) {
    if (closed_ || state_ != SessionState::identify || cid_.empty()) {
        return std::nullopt;
    }
    if (!challenge) {
        state_ = SessionState::normal;
        return SessionResult{};
    }
    if (challenge->size() > Limits{}.parameter_bytes || detail::base32_bytes(*challenge) < 24) {
        return std::nullopt;
    }
    std::string line = "IGPA ";
    line += *challenge;
    line += '\n';
    state_ = SessionState::verify;
    return SessionResult{SessionAction::none, {std::move(line)}};
}

std::optional<SessionResult> Session::complete_verification(bool accepted) {
    if (closed_ || state_ != SessionState::verify || !password_pending_) {
        return std::nullopt;
    }
    password_pending_ = false;
    if (!accepted) {
        return fatal_status("223", "Invalid password response");
    }
    state_ = SessionState::normal;
    return SessionResult{};
}

SessionResult Session::disconnect() {
    if (closed_) {
        return {SessionAction::close, {}};
    }
    if (state_ == SessionState::protocol) {
        return fatal_status("200", "Connection closed");
    }
    closed_ = true;
    return {SessionAction::close, {"IQUI " + sid_ + '\n'}};
}

SessionResult Session::fatal_status(std::string_view code, std::string_view description,
                                    std::string_view field) {
    closed_ = true;
    return {SessionAction::close, {status_line(code, description, field)}};
}

} // namespace dc24h::adc
