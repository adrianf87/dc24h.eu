#include "dc24h/adc/login.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/status.hpp"

#include <deque>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dc24h::adc {
struct LoginHub::Peer {
    struct Frame { Message message; std::string wire; Completion completion; };
    Peer(std::shared_ptr<SidPool> pool, std::string_view address, const users::Session& model)
        : session(std::move(pool), address), lifecycle(model) {}
    ClientSession session;
    const users::Session& lifecycle;
    std::deque<Frame> output;
    std::size_t offset{0};
    std::size_t queued{0};
    bool quit_queued{false};
};
namespace {
void check_model(users::MutationResult result) {
    if (result) { throw std::logic_error("Session lifecycle transition rejected"); }
}
users::CloseCause session_cause(CloseReason reason) noexcept {
    switch (reason) {
    case CloseReason::login_timeout: return users::CloseCause::login_timeout;
    case CloseReason::idle_timeout: return users::CloseCause::idle_timeout;
    case CloseReason::partial_frame_timeout: return users::CloseCause::partial_frame_timeout;
    case CloseReason::security_quota: return users::CloseCause::security_quota;
    case CloseReason::negotiation_limit: return users::CloseCause::negotiation_limit;
    case CloseReason::identity_limit: return users::CloseCause::identity_limit;
    }
    return users::CloseCause::internal_error;
}
Message info_message(std::string command, Command kind, std::vector<std::string> parameters) {
    return {MessageType::info, std::move(command), kind, {}, {}, {}, std::move(parameters), {}};
}
users::User public_user(const ClientInfo& metadata, std::string_view sid) {
    auto identity=users::Identity::create(sid,*metadata.field("ID"),*metadata.field("NI"));
    users::ProfileData data;
    data.description=metadata.field("DE").value_or("");
    data.email=metadata.field("EM").value_or("");
    data.application=metadata.field("AP").value_or("");
    data.version=metadata.field("VE").value_or("");
    data.locale=metadata.field("LO").value_or("");
    data.shared_bytes=metadata.integer("SS");
    data.shared_files=metadata.integer("SF");
    data.slots=metadata.integer("SL");
    auto features=metadata.field("SU").value_or("");
    while (!features.empty()) {
        const auto comma=features.find(',');
        data.features.emplace_back(features.substr(0,comma));
        if (comma==std::string_view::npos) { break; }
        features.remove_prefix(comma+1);
    }
    auto profile=users::Profile::create(std::move(data));
    if (!identity || !profile) { throw std::logic_error("Validated metadata exceeds user model bounds"); }
    auto user=users::User::create(std::move(*identity),std::move(*profile));
    if (!user) { throw std::logic_error("Invalid validated public user"); }
    return std::move(*user);
}
std::string_view error_status(const ClientSessionError& error) {
    return std::visit([](auto value) -> std::string_view {
        using T = decltype(value);
        if constexpr (std::is_same_v<T, InfError>) { return inf_status_code(value); }
        else if constexpr (std::is_same_v<T, SupportError>) { return support_status_code(value); }
        else if constexpr (std::is_same_v<T, SidError>) { return sid_status_code(value); }
        else { return value == StateError::invalid_state || value == StateError::invalid_sequence ? "244" : "240"; }
    }, error);
}
std::string_view status_description(std::string_view code) noexcept {
    if (code == "211") { return "Hub session limit reached"; }
    if (code == "221") { return "Invalid nickname"; }
    if (code == "222") { return "Nickname already in use"; }
    if (code == "224") { return "Client ID already in use"; }
    if (code == "227") { return "Invalid client identity"; }
    if (code == "243") { return "Required client information missing or invalid"; }
    if (code == "244") { return "Invalid connection state"; }
    if (code == "245") { return "Required feature missing"; }
    if (code == "246") { return "Invalid client address"; }
    if (code == "247") { return "No common session hash"; }
    if (code == "210") { return "Hub internal error"; }
    return "Protocol error";
}
} // namespace

LoginHub::LoginHub(LoginLimits limits) : limits_(limits), pool_(std::make_shared<SidPool>()), sessions_(limits.connections) {
    if (limits.connections == 0 || limits.connections > 65535 ||
        limits.output_per_client < 128 || limits.output_per_client > 16 * 1024 * 1024 ||
        limits.output_total < limits.output_per_client || limits.output_total > 256 * 1024 * 1024) {
        throw std::invalid_argument("Invalid login/output limits");
    }
}
LoginHub::~LoginHub() = default;

void LoginHub::connect(Id id, std::string_view address, users::Transport transport, users::TimePoint accepted) {
    const auto key = users::SessionId::from_value(id);
    if (!key) { throw std::logic_error("Zero login connection ID"); }
    const auto registered = sessions_.add(*key,transport,accepted);
    const auto model = std::get_if<const users::Session*>(&registered);
    if (!model) { throw std::logic_error("Duplicate, reused or excess login connection"); }
    try {
        auto peer = std::make_unique<Peer>(pool_, address, **model);
        if (transport == users::Transport::adc) { check_model(sessions_.start_negotiation(peer->lifecycle.id(),accepted)); }
        peers_.emplace(id,std::move(peer));
    } catch (...) {
        // Failed protocol allocation leaves no orphan registry entry. The ID
        // remains consumed, so no stale reference can identify a later peer.
        check_model(sessions_.finish_close((*model)->id(),users::CloseCause::internal_error,accepted));
        (void)sessions_.remove(*key);
        throw;
    }
}
void LoginHub::transport_ready(Id id, users::TimePoint now) {
    check_model(sessions_.start_negotiation(peers_.at(id)->lifecycle.id(),now));
}

bool LoginHub::enqueue(Peer& peer, Message message, Completion completion) {
    if (peer.lifecycle.immediate() || peer.quit_queued) { return false; }
    auto encoded = serialize(message);
    auto* wire = std::get_if<std::string>(&encoded);
    if (!wire) { throw std::logic_error("Invalid locally generated ADC output"); }
    if (wire->size() > limits_.output_per_client - peer.queued || wire->size() > limits_.output_total - queued_) {
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::output_limit,users::Clock::now()));
        // Owner closes immediately without disturbing a pinned TLS front.
        return false;
    }
    const auto size = wire->size();
    peer.output.push_back({std::move(message), std::move(*wire), completion});
    peer.queued += size;
    queued_ += size;
    return true;
}
void LoginHub::reject(Peer& peer, std::string_view status, std::optional<std::string_view> command,
                      std::string_view description, users::CloseCause cause, users::TimePoint now) {
    if (peer.lifecycle.closing() || peer.lifecycle.immediate()) { return; }
    check_model(sessions_.begin_close(peer.lifecycle.id(),cause,now));
    // Keep a possibly partially sent/TLS-pinned front, cancel all later ordinary
    // frames. Never mutate the immutable front pointer or its remaining length.
    while (peer.output.size() > 1) {
        queued_ -= peer.output.back().wire.size();
        peer.queued -= peer.output.back().wire.size();
        peer.output.pop_back();
    }
    std::optional<Sid> sid = Sid::parse(peer.session.sid());
    if (!sid && !peer.output.empty() && peer.output.front().message.kind == Command::sid) {
        // This already queued ISID must complete ahead of the terminal status.
        sid = Sid::parse(peer.output.front().message.parameters[0]);
    }
    if (description.empty()) { description = status_description(status); }
    if (!enqueue(peer, make_status(*StatusCode::parse(status), description, command))) { return; }
    QuitOptions options{};
    options.message = description;
    if (sid && enqueue(peer, make_quit(*sid, options))) { peer.quit_queued = true; }
}
void LoginHub::close(Id id, CloseReason reason, users::TimePoint now) {
    auto& peer = *peers_.at(id);
    if (peer.lifecycle.phase() == users::Phase::accepted) {
        // An unready TLS transport cannot carry an application notice.
        check_model(sessions_.force_close(peer.lifecycle.id(),session_cause(reason),now));
        return;
    }
    switch (reason) {
    case CloseReason::login_timeout: reject(peer, "240", {}, "Login timed out", users::CloseCause::login_timeout, now); break;
    case CloseReason::idle_timeout: reject(peer, "230", {}, "Idle connection timed out", users::CloseCause::idle_timeout, now); break;
    case CloseReason::partial_frame_timeout: reject(peer, "240", {}, "Incomplete frame timed out", users::CloseCause::partial_frame_timeout, now); break;
    case CloseReason::security_quota: reject(peer, "240", {}, "Protocol resource limit exceeded", users::CloseCause::security_quota, now); break;
    case CloseReason::negotiation_limit: reject(peer, "240", {}, "Negotiation resource limit exceeded", users::CloseCause::negotiation_limit, now); break;
    case CloseReason::identity_limit: reject(peer, "240", {}, "Identity verification rate limit exceeded", users::CloseCause::identity_limit, now); break;
    }
}
bool LoginHub::needs_identity_check(Id id, const Message& message) const {
    const auto& peer = *peers_.at(id);
    return !peer.lifecycle.closing() && !peer.lifecycle.immediate() && peer.session.state() == ConnectionState::identify &&
        !peer.session.info() && message.kind == Command::inf && message.command == "INF" &&
        message.type == MessageType::broadcast;
}
void LoginHub::relay(Id except, const Message& message) {
    for (auto& [id, peer] : peers_) {
        if (id != except && peer->lifecycle.identity() != nullptr && !peer->lifecycle.closing() && !peer->lifecycle.immediate()) { (void)enqueue(*peer, message); }
    }
}

void LoginHub::receive(Id id, const Message& message) {
    auto& peer = *peers_.at(id);
    if (peer.lifecycle.closing() || peer.lifecycle.immediate()) { return; }
    if (peer.lifecycle.phase() == users::Phase::accepted) {
        throw std::logic_error("Application input before transport readiness");
    }
    const auto before = peer.session.state();
    const auto previous = message.kind == Command::inf ? peer.session.info() : std::optional<ClientInfo>{};
    const std::string fourcc(1, static_cast<char>(message.type));
    const std::string offending = fourcc + message.command;
    if (before == ConnectionState::normal && !peer.lifecycle.ready()) { reject(peer, "244", offending); return; }
    if (message.kind == Command::inf && message.type == MessageType::broadcast) {
        // Check prospective nickname before the session commits an update.
        for (const auto& token : message.parameters) {
            const auto field = parse_named_parameter(token);
            if (field && field->name == "NI") {
                const auto* found = sessions_.by_nickname(field->value);
                if (found && found->id().value() != id) { reject(peer, "222"); return; }
            }
        }
    }
    if (const auto error = peer.session.observe(Direction::from_client, message)) {
        const auto* state_error = std::get_if<StateError>(&*error);
        if (before == ConnectionState::normal && state_error && *state_error == StateError::unsupported_command) {
            (void)enqueue(peer, make_status(*StatusCode::parse("125"), "Command not implemented", offending));
            return;
        }
        const auto code = error_status(*error);
        const auto* support_error = std::get_if<SupportError>(&*error);
        const auto feature = support_error && *support_error == SupportError::session_hash_removed ? "TIGR" : "BASE";
        reject(peer, code, code == "245" ? std::optional<std::string_view>{feature} : std::optional<std::string_view>{offending});
        return;
    }
    if (peer.session.state() == ConnectionState::disconnected) {
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::peer_status,users::Clock::now())); return;
    }
    if (before == ConnectionState::protocol && message.kind == Command::sup) {
        (void)enqueue(peer, *peer.session.support().initial_reply(), Completion::support);
    } else if (message.kind == Command::inf) {
        const auto& metadata = *peer.session.info();
        const std::string cid(*metadata.field("ID"));
        const auto* duplicate = sessions_.by_cid(cid);
        if (duplicate && duplicate->id().value() != id) { reject(peer, "224"); return; }
        if (before == ConnectionState::identify) {
            // Accounts are not implemented: this release explicitly admits guests.
            // CID proof is not an account/password/role authentication mechanism.
            if (peer.session.approve_identity(IdentityMode::guest)) { reject(peer, "240"); return; }
            if (!enqueue(peer, info_message("INF", Command::inf, {"CT32", "NIdc24h.eu", "DEGuest login alpha"}))) { return; }
            for (const auto& [other, existing] : peers_) {
                if (other != id && existing->lifecycle.identity() != nullptr && !existing->lifecycle.closing() && !existing->lifecycle.immediate()) {
                    if (!enqueue(peer, existing->session.info()->public_message(*Sid::parse(existing->session.sid())))) { return; }
                }
            }
            // Include pending admissions in snapshots and relays. FIFO ordering
            // preserves welcome-before-join and avoids simultaneous-login gaps.
            const auto public_info = metadata.public_message(*Sid::parse(peer.session.sid()));
            if (!enqueue(peer, public_info, Completion::welcome)) { return; } // Own INF is last in the initial list.
            check_model(sessions_.identify(peer.lifecycle.id(),public_user(metadata,peer.session.sid()),users::Clock::now()));
            relay(id, public_info);
        } else {
            check_model(sessions_.update_user(peer.lifecycle.id(),public_user(metadata,peer.session.sid()),users::Clock::now()));
            // A complete public snapshot also conveys optional field cancellations.
            // Relay the sanitized update below, preserving bare cancellation names.
            auto update = metadata.public_message(*Sid::parse(peer.session.sid()));
            for (const auto& token : previous->public_message(*Sid::parse(peer.lifecycle.identity()->sid())).parameters) {
                const auto field = *parse_named_parameter(token);
                if (!metadata.field(field.name)) { update.parameters.emplace_back(field.name); }
            }
            relay(0, update); // Transport IDs are nonzero; includes the sender.
        }
    } else if (message.kind == Command::sta) {
        if (message.type != MessageType::hub) { route_status(id, message); }
    } else if (before == ConnectionState::normal && message.kind != Command::sup) {
        // Passing the state gate does not implement chat/search/transfers.
        (void)enqueue(peer, make_status(*StatusCode::parse("125"), "Command not implemented", offending));
    }
}
void LoginHub::route_status(Id id, const Message& message) {
    const auto safe = public_status(message);
    if (message.type == MessageType::direct || message.type == MessageType::echo) {
        const auto* target = sessions_.by_sid(*message.target);
        if (!target || !target->ready()) {
            (void)enqueue(*peers_.at(id), make_status(*StatusCode::parse("140"), "Status recipient unavailable"));
            return;
        }
        (void)enqueue(*peers_.at(target->id().value()), safe);
        if (message.type == MessageType::echo && target->id().value() != id) { (void)enqueue(*peers_.at(id), safe); }
        return;
    }
    for (auto& [recipient, peer] : peers_) {
        if (!ready(recipient)) { continue; }
        if (message.type == MessageType::feature) {
            const auto supported = peer->session.info()->field("SU").value_or("");
            const auto has_feature = [&](std::string_view name) {
                std::size_t start=0;
                while (start < supported.size()) {
                    const auto comma=supported.find(',',start);
                    if (supported.substr(start, comma==std::string_view::npos ? comma : comma-start)==name) { return true; }
                    if (comma==std::string_view::npos) { break; }
                    start=comma+1;
                }
                return false;
            };
            bool matches=true;
            for (const auto& feature : message.features) {
                if (has_feature(feature.name) != feature.required) { matches=false; break; }
            }
            if (!matches) { continue; }
        }
        (void)enqueue(*peer, safe);
    }
}

std::string_view LoginHub::output(Id id) const {
    const auto& peer = *peers_.at(id);
    return peer.output.empty() ? std::string_view{} : std::string_view(peer.output.front().wire).substr(peer.offset);
}
bool LoginHub::consume(Id id, std::size_t bytes) {
    auto& peer = *peers_.at(id);
    if (peer.output.empty() || bytes == 0 || bytes > peer.output.front().wire.size() - peer.offset) {
        throw std::logic_error("Invalid transport send completion");
    }
    peer.offset += bytes;
    if (peer.offset != peer.output.front().wire.size()) { return false; }
    auto frame = std::move(peer.output.front());
    peer.output.pop_front();
    peer.offset = 0;
    peer.queued -= frame.wire.size();
    queued_ -= frame.wire.size();
    if (peer.lifecycle.closing()) { return false; }
    if (peer.session.observe(Direction::to_client, frame.message)) {
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now())); return false;
    }
    if (frame.completion == Completion::support) {
        if (const auto error = peer.session.prepare_sid()) { reject(peer, sid_status_code(*error)); return false; }
        (void)enqueue(peer, *peer.session.sid_reply());
    } else if (frame.completion == Completion::welcome) {
        check_model(sessions_.mark_ready(peer.lifecycle.id(),users::Clock::now())); return true;
    }
    return false;
}
void LoginHub::disconnect(Id id, bool announce, users::CloseCause cause, users::TimePoint now) {
    const auto found = peers_.find(id);
    if (found == peers_.end()) { return; }
    auto& peer = *found->second;
    if (announce && peer.lifecycle.identity() != nullptr) { relay(id, make_quit(*Sid::parse(peer.lifecycle.identity()->sid()))); }
    queued_ -= peer.queued;
    peer.session.disconnect();
    check_model(sessions_.finish_close(peer.lifecycle.id(),cause,now));
    peers_.erase(found); // Drop protocol/output references before deleting the model.
    if (sessions_.remove(*users::SessionId::from_value(id))) { throw std::logic_error("Session registry cleanup failed"); }
}
bool LoginHub::closing(Id id) const { return session_model(id).closing(); }
bool LoginHub::must_close(Id id) const {
    const auto& peer = *peers_.at(id);
    return peer.lifecycle.immediate() || (peer.lifecycle.closing() && peer.output.empty());
}
bool LoginHub::ready(Id id) const { return session_model(id).ready(); }
bool LoginHub::overloaded(Id id) const { return session_model(id).terminal_cause() == users::CloseCause::output_limit; }
bool LoginHub::peer_fatal(Id id) const { return session_model(id).terminal_cause() == users::CloseCause::peer_status; }
const users::Session& LoginHub::session_model(Id id) const {
    const auto key=users::SessionId::from_value(id);
    const auto* model=key ? sessions_.find(*key) : nullptr;
    if (!model) { throw std::out_of_range("Missing session"); }
    return *model;
}
std::optional<users::Snapshot> LoginHub::snapshot(Id id) const {
    const auto key=users::SessionId::from_value(id);
    return key ? sessions_.snapshot(*key) : std::nullopt;
}
ConnectionState LoginHub::state(Id id) const { return peers_.at(id)->session.state(); }
} // namespace dc24h::adc
