#include "dc24h/adc/login.hpp"
#include "dc24h/adc/sch.hpp"
#include "dc24h/adc/res.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/status.hpp"
#include "dc24h/adc/connection_errors.hpp"
#include "dc24h/adc/msg.hpp"

#include <deque>
#include <algorithm>
#include <new>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dc24h::adc {
struct LoginHub::Peer {
    struct Frame {
        struct Owned { Message message; std::string wire; };
        std::variant<Owned,SharedPublicChatFrame,SharedPrivateChatFrame,SharedHubMessageFrame,SharedSearchFrame,SharedResultFrame,SharedConnectionFrame,SharedStatusFrame> content;
        Completion completion;
        [[nodiscard]] const Message& message() const noexcept {
            return std::visit([](const auto& frame) -> const Message& {
                if constexpr (std::is_same_v<std::decay_t<decltype(frame)>, Owned>) { return frame.message; }
                else { return frame->message(); }
            }, content);
        }
        [[nodiscard]] std::string_view wire() const noexcept {
            return std::visit([](const auto& frame) -> std::string_view {
                if constexpr (std::is_same_v<std::decay_t<decltype(frame)>, Owned>) { return frame.wire; }
                else { return frame->wire(); }
            }, content);
        }
    };
    Peer(std::shared_ptr<SidPool> pool, std::string_view address, const users::Session& model,
         const std::optional<chat::FloodLimits>& limits, const search::SecuritySettings& search_settings,
         const transfers::SecuritySettings& transfer_settings)
        : session(std::move(pool), address), lifecycle(model), search_guard(search_settings), search_permissions(search_settings.guests),
          transfer_permissions(transfer_settings.guests), transfer_guard(transfer_settings.flood) { if (limits) { anti_flood.emplace(*limits); } }
    ClientSession session;
    PasswordChallenge authentication;
    std::optional<std::uint64_t> pending_account;
    std::optional<accounts::AccountId> verified_account;
    const users::Session& lifecycle;
    std::deque<Frame> output;
    std::size_t offset{0};
    std::size_t queued{0};
    bool quit_queued{false};
    std::optional<chat::FloodGuard> anti_flood;
    search::SearchGuard search_guard;
    search::Permissions search_permissions;
    transfers::Permissions transfer_permissions;
    transfers::TransferGuard transfer_guard;
};
namespace {
std::size_t message_bytes(const Message& message) noexcept {
    constexpr auto cap = max_frame_bytes + 1;
    if (message.parameters.size() > max_parameters || message.features.size() > max_feature_selectors) { return std::numeric_limits<std::size_t>::max(); }
    std::size_t bytes = 4;
    const auto add = [&](std::size_t size) { bytes = size >= cap - bytes ? cap : bytes + size; };
    if (message.source) { add(1); add(message.source->size()); }
    if (message.target) { add(1); add(message.target->size()); }
    if (!message.features.empty()) { add(1); }
    for (const auto& feature : message.features) { add(1); add(feature.name.size()); }
    for (const auto& field : message.parameters) { add(1); add(field.size()); }
    return bytes == cap ? std::numeric_limits<std::size_t>::max() : bytes;
}
SharedHubMessageFrame welcome_packet(const std::optional<std::string>& text) {
    if (!text) { return {}; }
    chat::validate_welcome(*text);
    auto prepared = make_hub_message(*text);
    if (auto* frame = std::get_if<SharedHubMessageFrame>(&prepared)) { return std::move(*frame); }
    throw std::invalid_argument("Invalid chat welcome message");
}
void check_model(users::MutationResult result) {
    if (result) { throw std::logic_error("Session lifecycle transition rejected"); }
}
bool public_member(const users::Session& model) noexcept {
    return model.phase()==users::Phase::identified || model.phase()==users::Phase::ready;
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
    if (code == "223") { return "Authentication failed"; }
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

LoginHub::LoginHub(LoginLimits limits) : limits_(limits), pool_(std::make_shared<SidPool>()), sessions_(limits.users.bounded_by(limits.connections)), search_(sessions_,limits.search.tokens), search_router_(search_), result_router_(search_), transfers_(sessions_,limits.transfers,this), connection_router_(transfers_), status_router_(sessions_), chat_(sessions_), message_router_(chat_,limits.chat), hub_messages_(sessions_), welcome_message_(welcome_packet(limits.welcome_text)), user_login_(sessions_), user_list_(sessions_), inf_updates_(sessions_), user_disconnect_(sessions_) {
    search::validate_security(limits.search);
    transfers::validate_limits(limits.transfers.flood);
    if (limits.anti_flood) { chat::validate(*limits.anti_flood); }
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
        auto peer = std::make_unique<Peer>(pool_, address, **model, limits_.anti_flood, limits_.search, limits_.transfers);
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
    peer.output.push_back({Peer::Frame::Owned{std::move(message), std::move(*wire)}, completion});
    peer.queued += size;
    queued_ += size;
    return true;
}
template<class Packet>
bool LoginHub::enqueue_shared(Peer& peer, std::shared_ptr<const Packet> frame) {
    if (peer.lifecycle.immediate() || peer.lifecycle.closing() || peer.quit_queued) { return false; }
    const auto size=frame->wire().size();
    if (size>limits_.output_per_client-peer.queued || size>limits_.output_total-queued_) {
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::output_limit,users::Clock::now()));
        return false;
    }
    // No message copy or serialization per recipient; the front remains pinned
    // through partial TCP/TLS sends while other queues release their references.
    peer.output.push_back({std::move(frame),Completion::observe});
    peer.queued+=size;
    queued_+=size;
    return true;
}
void LoginHub::reject(Peer& peer, std::string_view status, std::optional<std::string_view> command,
                      std::string_view description, users::CloseCause cause, users::TimePoint now) {
    peer.authentication.cancel();
    if (peer.lifecycle.closing() || peer.lifecycle.immediate()) { return; }
    check_model(sessions_.begin_close(peer.lifecycle.id(),cause,now));
    // Keep a possibly partially sent/TLS-pinned front, cancel all later ordinary
    // frames. Never mutate the immutable front pointer or its remaining length.
    while (peer.output.size() > 1) {
        queued_ -= peer.output.back().wire().size();
        peer.queued -= peer.output.back().wire().size();
        peer.output.pop_back();
    }
    std::optional<Sid> sid = Sid::parse(peer.session.sid());
    if (!sid && !peer.output.empty() && peer.output.front().message().kind == Command::sid) {
        // This already queued ISID must complete ahead of the terminal status.
        sid = Sid::parse(peer.output.front().message().parameters[0]);
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
    try {
        switch (reason) {
        case CloseReason::login_timeout: reject(peer, "240", {}, "Login timed out", users::CloseCause::login_timeout, now); break;
        case CloseReason::idle_timeout: reject(peer, "230", {}, "Idle connection timed out", users::CloseCause::idle_timeout, now); break;
        case CloseReason::partial_frame_timeout: reject(peer, "240", {}, "Incomplete frame timed out", users::CloseCause::partial_frame_timeout, now); break;
        case CloseReason::security_quota: reject(peer, "240", {}, "Protocol resource limit exceeded", users::CloseCause::security_quota, now); break;
        case CloseReason::negotiation_limit: reject(peer, "240", {}, "Negotiation resource limit exceeded", users::CloseCause::negotiation_limit, now); break;
        case CloseReason::identity_limit: reject(peer, "240", {}, "Identity verification rate limit exceeded", users::CloseCause::identity_limit, now); break;
        }
    } catch (const std::bad_alloc&) {
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,now));
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
        if (id != except && public_member(peer->lifecycle)) { (void)enqueue(*peer, message); }
    }
}

void LoginHub::receive(Id id, const Message& message, chat::FloodClock::time_point chat_now) {
    const auto found = peers_.find(id);
    const bool connection_request = message.kind == Command::ctm || message.kind == Command::rcm;
    const bool eligible = found != peers_.end() && !found->second->lifecycle.closing() && !found->second->lifecycle.immediate();
    transfers::Attempt transfer_attempt(connection_request && eligible ? &transfer_statistics_ : nullptr,
        message.kind == Command::ctm ? transfers::Operation::ctm : transfers::Operation::rcm);
    if (message.kind != Command::sta && message.kind != Command::ctm && message.kind != Command::rcm) {
        receive_impl(id, message, chat_now, transfer_attempt); return;
    }
    try { receive_impl(id, message, chat_now, transfer_attempt); }
    catch (const std::bad_alloc&) {
        // Include validation and terminal diagnostics in source fault isolation.
        transfer_attempt.outcome(transfers::Outcome::error);
        if (found != peers_.end()) {
            check_model(sessions_.force_close(found->second->lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
        }
    }
}
void LoginHub::receive_impl(Id id, const Message& message, chat::FloodClock::time_point chat_now, transfers::Attempt& transfer_attempt) {
    auto& peer = *peers_.at(id);
    if (peer.lifecycle.closing() || peer.lifecycle.immediate()) { return; }
    search::Attempt search_attempt(message.kind==Command::sch || message.kind==Command::res ? &search_statistics_ : nullptr,
        message.kind==Command::res ? search::Kind::result : search::Kind::query);
    chat::Attempt attempt(message.kind == Command::msg &&
        (message.type == MessageType::broadcast || message.type == MessageType::direct || message.type == MessageType::echo)
        ? &chat_statistics_ : nullptr);
    if (peer.lifecycle.phase() == users::Phase::accepted) {
        throw std::logic_error("Application input before transport readiness");
    }
    const auto before = peer.session.state();
    const std::string fourcc(1, static_cast<char>(message.type));
    const std::string offending = fourcc + message.command;
    if (before == ConnectionState::normal && !peer.lifecycle.ready()) { reject(peer, "244", offending); return; }
    if (message.kind == Command::inf && message.type == MessageType::broadcast) {
        // Check prospective nickname before the session commits an update.
        for (const auto& token : message.parameters) {
            const auto field = parse_named_parameter(token);
            if (field && field->name == "NI") {
                if (before == ConnectionState::normal && limits_.authentication &&
                    !limits_.authentication->allows_identity(field->value,*peer.session.info()->field("ID"),authenticated_account(id))) {
                    reject(peer,"223"); return;
                }
                const auto* found = sessions_.by_nickname(field->value);
                if (found && found->id().value() != id) { reject(peer, "222"); return; }
            }
        }
    }
    if (before == ConnectionState::normal && message.kind == Command::inf) {
        std::optional<InfUpdateResult> update;
        try {
            update.emplace(inf_updates_.apply(peer.lifecycle.id(),peer.session,message,users::Clock::now()));
        } catch (const std::bad_alloc&) {
            check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
            return;
        }
        if (const auto* error = std::get_if<InfUpdateFailure>(&*update)) {
            reject(peer,inf_update_status(*error),offending); return;
        }
        if (const auto& delta = std::get<AppliedInfUpdate>(*update).message) { relay(0,*delta); }
        return;
    }
    ClientSessionResult observation;
    try { observation=peer.session.observe(Direction::from_client,message); }
    catch (const std::bad_alloc&) {
        if (message.kind==Command::sch || message.kind==Command::res) {
            search_attempt.outcome(search::Outcome::error);
            check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));return;
        }
        if (message.kind==Command::ctm || message.kind==Command::rcm) {
            transfer_attempt.outcome(transfers::Outcome::error);
            check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));return;
        }
        if (message.kind!=Command::msg) { throw; }
        attempt.outcome(chat::Event::internal_error);
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
        return;
    }
    if (const auto error = observation) {
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
    } else if (before == ConnectionState::verify && message.kind == Command::pas) {
        if (!peer.pending_account || peer.authentication.verify(message)) { reject(peer,"223"); return; }
        if (peer.session.approve_password()) { reject(peer,"244"); return; }
        peer.verified_account = accounts::AccountId::from_value(*peer.pending_account);
        peer.pending_account.reset();
        complete_login(peer);
    } else if (message.kind == Command::inf) {
        const auto& metadata = *peer.session.info();
        const std::string cid(*metadata.field("ID"));
        const auto* duplicate = sessions_.by_cid(cid);
        if (duplicate && duplicate->id().value() != id) { reject(peer, "224"); return; }
        if (before == ConnectionState::identify) {
            // The manager enforces this again on registration. Preflight before
            // welcome planning/queueing so a full hub discloses no user list.
            if (sessions_.users_full()) { reject(peer,"211",{},"Hub user limit reached"); return; }
            if (limits_.authentication) {
                try {
                    auto decision = limits_.authentication->prepare(*metadata.field("NI"),cid,peer.lifecycle.transport(),peer.authentication);
                    if (std::holds_alternative<DeniedAuthentication>(decision)) { reject(peer,"223"); return; }
                    if (auto* required = std::get_if<PasswordAuthentication>(&decision)) {
                        if (required->account_id == 0 || required->account_id > 9223372036854775807ULL || !peer.authentication.pending() ||
                            peer.session.approve_identity(IdentityMode::password_required)) { reject(peer,"210"); return; }
                        peer.pending_account = required->account_id;
                        (void)enqueue(peer,std::move(required->challenge),Completion::authentication);
                        return;
                    }
                } catch (const std::exception&) { reject(peer,"210"); return; }
            }
            if (peer.session.approve_identity(IdentityMode::guest)) { reject(peer, "240"); return; }
            complete_login(peer);
        }
    } else if (message.kind == Command::msg) {
        if (peer.anti_flood && (message.type == MessageType::broadcast || message.type == MessageType::direct || message.type == MessageType::echo)) {
            // Direction/state/source/known-field checks above take precedence.
            // Denied traffic never constructs a fanout plan or private echo.
            const auto decision = peer.anti_flood->admit(message.parameters[0].size(), chat_now);
            if (!decision.allowed) {
                attempt.outcome(chat::Event::flood_limited);
                if (decision.notify) {
                    try { (void)enqueue(peer, make_status(*StatusCode::parse("100"), "Chat message rate limit exceeded", offending)); }
                    catch (const std::bad_alloc&) {
                        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
                    }
                }
                return;
            }
        }
        route_chat(id,message,attempt);
    } else if (message.kind == Command::sch) {
        receive_search(id, message, chat_now, search_attempt);
    } else if (message.kind == Command::res) {
        receive_result(id, message, chat_now, search_attempt);
    } else if (message.kind == Command::ctm || message.kind == Command::rcm) {
        receive_connection(id, message, chat_now, transfer_attempt);
    } else if (message.kind == Command::sta) {
        if (message.type != MessageType::hub) { route_status(id, message); }
    } else if (before == ConnectionState::normal && message.kind != Command::sup) {
        // Transfer handlers remain separate.
        (void)enqueue(peer, make_status(*StatusCode::parse("125"), "Command not implemented", offending));
    }
}
void LoginHub::complete_login(Peer& peer) {
    std::optional<InitialUserList> welcome;
    try {
        const auto available = std::min(limits_.output_per_client - peer.queued, limits_.output_total - queued_);
        const auto notice_bytes = welcome_message_ ? welcome_message_->wire().size() : 0;
        if (notice_bytes > available) {
            check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::output_limit,users::Clock::now()));
            return;
        }
        auto result = user_list_.prepare(peer.lifecycle.id(), peer.session,
            [this](users::SessionId owner) -> const ClientInfo* {
                const auto found = peers_.find(owner.value());
                return found != peers_.end() && found->second->session.info() ? &*found->second->session.info() : nullptr;
            }, available - notice_bytes);
        if (const auto* error = std::get_if<UserListError>(&result)) {
            const auto cause = *error == UserListError::output_limit ? users::CloseCause::output_limit : users::CloseCause::internal_error;
            check_model(sessions_.force_close(peer.lifecycle.id(),cause,users::Clock::now()));
            return;
        }
        welcome.emplace(std::move(std::get<InitialUserList>(result)));
        const auto& frames = welcome->frames();
        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (!enqueue(peer, frames[i], i + 1 == frames.size() ? Completion::welcome : Completion::observe)) { return; }
        }
        // Client sees its own INF before system chat. Queue before the
        // registry commit/publication; readiness still ends at own INF.
        if (welcome_message_ && !enqueue_shared(peer, welcome_message_)) { return; }
        if (const auto error = user_login_.register_approved(peer.lifecycle.id(), peer.session, users::Clock::now())) {
            reject(peer, user_login_status(*error)); return;
        }
    } catch (const std::bad_alloc&) {
        // Welcome planning/queueing and registration failures publish no
        // user. Preserve pinned output until immediate transport cleanup.
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
        return;
    }
    // Publication follows the complete prepared welcome and registry
    // commit. Subsequent FIFO joins/updates/quits follow own INF.
    relay(peer.lifecycle.id().value(), welcome->frames().back());
}
std::optional<users::AuthSession> LoginHub::authenticated_session(Id id) const noexcept {
    const auto key = users::SessionId::from_value(id);
    return key ? sessions_.authenticated(*key) : std::nullopt;
}
std::optional<std::uint64_t> LoginHub::authenticated_account(Id id) const {
    const auto auth = authenticated_session(id);
    return auth ? std::optional<std::uint64_t>{auth->account_id.value()} : std::nullopt;
}
bool LoginHub::revoke_auth_session(Id id, users::TimePoint now) noexcept {
    const auto auth = authenticated_session(id);
    if (!auth || sessions_.force_close(auth->session_id,users::CloseCause::authentication_revoked,now)) { return false; }
    (void)search_.forget_searches(auth->session_id);
    return true;
}
void LoginHub::receive_connection(Id id, const Message& message, users::TimePoint now, transfers::Attempt& attempt) {
    auto& peer = *peers_.at(id);
    try {
        const auto command = std::string(1, static_cast<char>(message.type)) + (message.kind == Command::ctm ? "CTM" : "RCM");
        const auto limited = [&] {
            if (peer.transfer_guard.notice(now)) {
                (void)enqueue(peer, make_status(*StatusCode::parse("100"), "Connection request rate limit exceeded", command));
            }
        };
        const auto bytes = message_bytes(message);
        // Session/state/source checks still run before admission.
        // Charge every valid request attempt before routing/policy diagnostics.
        if (!peer.transfer_guard.admit(transfers::Direction::outgoing, bytes, now)) {
            attempt.outcome(transfers::Outcome::rate_limited); limited(); return;
        }
        const auto refuse = [&](ConnectionRoutingError error) {
            const auto failure = connection_failure(error, message.kind);
            if (failure.action == FailureAction::reply) {
                (void)enqueue(peer, make_status(*StatusCode::parse(failure.code), failure.description, command));
            } else if (failure.action == FailureAction::terminate) {
                reject(peer, failure.code, command, failure.description);
            } else {
                attempt.outcome(transfers::Outcome::error);
                check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
            }
        };
        auto prepared = connection_router_.prepare(peer.lifecycle.id(), message);
        if (const auto* error = std::get_if<ConnectionRoutingError>(&prepared)) { refuse(*error); return; }
        const auto& routed = std::get<RoutedConnection>(prepared);
        const auto target = peers_.find(routed.plan().target().value());
        if (target == peers_.end()) { (void)connection_router_.cancel(routed); refuse(ConnectionRoutingError::recipient_unavailable); return; }
        // All senders share this target lifetime's incoming budget. An E echo
        // is part of the same attempt, never an additional incoming request.
        if (!target->second->transfer_guard.admit(transfers::Direction::incoming, bytes, now)) {
            attempt.outcome(transfers::Outcome::recipient_limited);
            (void)connection_router_.cancel(routed); limited(); return;
        }
        const auto result = connection_router_.dispatch(routed, [&](users::SessionId recipient, const SharedConnectionFrame& frame) {
            const auto found = peers_.find(recipient.value());
            if (found == peers_.end()) { return false; }
            try { return enqueue_shared(*found->second, frame); }
            catch (const std::bad_alloc&) {
                attempt.queue_error();
                check_model(sessions_.force_close(recipient,users::CloseCause::internal_error,users::Clock::now())); return false;
            }
        });
        if (const auto* error = std::get_if<ConnectionRoutingError>(&result)) { refuse(*error); }
        else {
            const auto& report = std::get<ConnectionDispatchReport>(result);
            attempt.outcome(transfers::Outcome::routed);
            attempt.delivery(report.queued, report.refused, report.canceled);
        }
    } catch (const std::bad_alloc&) {
        attempt.outcome(transfers::Outcome::error);
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
    }
}
std::optional<transfers::Permissions> LoginHub::connection_permissions(users::SessionId id) const noexcept {
    const auto found = peers_.find(id.value());
    if (found == peers_.end() || !found->second->lifecycle.ready()) { return {}; }
    return found->second->transfer_permissions;
}
bool LoginHub::set_transfer_permissions(Id id, transfers::Permissions permissions) noexcept {
    const auto found = peers_.find(id);
    if (found == peers_.end() || !found->second->lifecycle.ready()) { return false; }
    found->second->transfer_permissions = permissions;
    return true;
}
bool LoginHub::set_search_permissions(Id id, search::Permissions permissions) noexcept {
    const auto found = peers_.find(id);
    if (found == peers_.end() || !found->second->lifecycle.ready()) { return false; }
    auto& peer = *found->second;
    if (peer.search_permissions.queries && !permissions.queries) { (void)search_.forget_searches(peer.lifecycle.id()); }
    peer.search_permissions = permissions;
    return true;
}
bool LoginHub::admit_search(Peer& peer, const Message& message, search::Operation operation, users::TimePoint now, search::Attempt& attempt) {
    std::string_view code = "100", description;
    if (const auto error = search::authorize(limits_.search,peer.search_permissions,operation)) {
        code = "125";
        description = *error == search::PermissionError::disabled ? "Search operation is disabled" : "Search operation not permitted";
    } else if (!peer.search_guard.admit(operation,message_bytes(message),now)) {
        description = "Search rate limit exceeded";
        attempt.outcome(search::Outcome::rate_limited);
    } else if (operation == search::Operation::result && limits_.search.require_result_token &&
        std::none_of(message.parameters.begin(),message.parameters.end(),[](const auto& value){return value.starts_with("TO");})) {
        description = "Search result token required";
    } else { return true; }
    if (peer.search_guard.notice(now)) {
        const auto command = std::string(1,static_cast<char>(message.type)) + message.command;
        (void)enqueue(peer,make_status(*StatusCode::parse(code),description,command));
    }
    return false;
}
void LoginHub::receive_result(Id id, const Message& message, users::TimePoint now, search::Attempt& attempt) {
    auto& peer = *peers_.at(id);
    try {
        const auto command = std::string(1, static_cast<char>(message.type)) + "RES";
        const auto refuse = [&](ResultRoutingError error) {
            switch (error) {
            case ResultRoutingError::invalid_state: reject(peer,"244",command); break;
            case ResultRoutingError::wrong_source: reject(peer,"240",command); break;
            case ResultRoutingError::recipient_unavailable:
                attempt.delivery(0,1,0);
                (void)enqueue(peer,make_status(*StatusCode::parse("140"),"Search result recipient unavailable",command)); break;
            case ResultRoutingError::inactive_search:
                (void)enqueue(peer,make_status(*StatusCode::parse("100"),"Search result token is not active",command)); break;
            case ResultRoutingError::invalid_message:
                (void)enqueue(peer,make_status(*StatusCode::parse("100"),"Invalid search result",command)); break;
            default:
                attempt.outcome(search::Outcome::error);
                check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now())); break;
            }
        };
        if (!admit_search(peer,message,search::Operation::result,now,attempt)) { return; }
        auto prepared = result_router_.prepare(peer.lifecycle.id(),message,now);
        if (const auto* error = std::get_if<ResultRoutingError>(&prepared)) { refuse(*error); return; }
        bool recipient_limited = false;
        const auto bytes = message_bytes(message);
        const auto report = result_router_.dispatch(std::get<RoutedResult>(prepared),[&](users::SessionId recipient,const SharedResultFrame& frame) {
            const auto found = peers_.find(recipient.value());
            if (found == peers_.end()) { return false; }
            if (!found->second->search_guard.admit(search::Operation::incoming_result,bytes,now)) { recipient_limited = true; return false; }
            try { return enqueue_shared(*found->second,frame); }
            catch (const std::bad_alloc&) {
                check_model(sessions_.force_close(recipient,users::CloseCause::internal_error,users::Clock::now())); return false;
            }
        },now);
        if (const auto* error = std::get_if<ResultRoutingError>(&report)) { refuse(*error); }
        else {
            const auto& count=std::get<search::DispatchReport>(report);
            attempt.outcome(recipient_limited?search::Outcome::recipient_limited:search::Outcome::routed);
            attempt.delivery(count.queued,count.unavailable,count.refused);
            if(recipient_limited && peer.search_guard.notice(now)) {
                (void)enqueue(peer,make_status(*StatusCode::parse("100"),"Search recipient rate limit exceeded",command));
            }
        }
    } catch (const std::bad_alloc&) {
        attempt.outcome(search::Outcome::error);
        check_model(sessions_.force_close(peer.lifecycle.id(), users::CloseCause::internal_error, users::Clock::now()));
    }
}
void LoginHub::receive_search(Id id, const Message& message, users::TimePoint now, search::Attempt& attempt) {
    auto& peer = *peers_.at(id);
    try {
        if (!admit_search(peer,message,search::Operation::query,now,attempt)) { return; }
        auto result = search_router_.prepare(peer.lifecycle.id(),message,now);
        const auto command = std::string(1, static_cast<char>(message.type)) + "SCH";
        if (const auto* error = std::get_if<SearchRoutingError>(&result)) {
            switch (*error) {
            case SearchRoutingError::no_criteria: attempt.outcome(search::Outcome::ignored);return;
            case SearchRoutingError::invalid_state: reject(peer,"244",command); return;
            case SearchRoutingError::wrong_source: reject(peer,"240",command); return;
            case SearchRoutingError::recipient_unavailable:
                attempt.delivery(0,1,0);
                (void)enqueue(peer,make_status(*StatusCode::parse("140"),"Search recipient unavailable",command)); return;
            case SearchRoutingError::duplicate_token:
                (void)enqueue(peer,make_status(*StatusCode::parse("100"),"Search token already active",command)); return;
            case SearchRoutingError::search_limit:
                (void)enqueue(peer,make_status(*StatusCode::parse("100"),"Active search limit reached",command)); return;
            default: (void)enqueue(peer,make_status(*StatusCode::parse("100"),"Invalid search query",command)); return;
            }
        }
        const auto& routed = std::get<RoutedSearch>(result);
        const auto report = search_router_.dispatch(routed,[&](users::SessionId recipient,const SharedSearchFrame& frame) {
            const auto found = peers_.find(recipient.value());
            if (found == peers_.end()) { return false; }
            try { return enqueue_shared(*found->second,frame); }
            catch (const std::bad_alloc&) {
                check_model(sessions_.force_close(recipient,users::CloseCause::internal_error,users::Clock::now())); return false;
            }
        },now);
        if (std::holds_alternative<SearchRoutingError>(report)) {
            attempt.outcome(search::Outcome::error);
            (void)search_router_.cancel(routed);
            check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
        } else {
            const auto& count=std::get<search::DispatchReport>(report);
            attempt.outcome(search::Outcome::routed);
            attempt.delivery(count.queued,count.unavailable,count.refused,count.canceled);
        }
    } catch (const std::bad_alloc&) {
        attempt.outcome(search::Outcome::error);
        (void)search_.forget_searches(peer.lifecycle.id());
        check_model(sessions_.force_close(peer.lifecycle.id(), users::CloseCause::internal_error, users::Clock::now()));
    }
}
void LoginHub::route_chat(Id id, const Message& message, chat::Attempt& attempt) {
    auto& sender=*peers_.at(id);
    try {
        const std::string command=std::string(1,static_cast<char>(message.type))+"MSG";
        const auto unavailable=[&] {
            (void)enqueue(sender,make_status(*StatusCode::parse("140"),"Private message recipient unavailable",command));
        };
        auto prepared=message_router_.prepare(sender.lifecycle.id(),message);
        if (const auto* error=std::get_if<RoutingError>(&prepared)) {
            switch (*error) {
            case RoutingError::recipient_unavailable: unavailable();break;
            case RoutingError::text_limit:
                (void)enqueue(sender,make_status(*StatusCode::parse("100"),"Chat text limit exceeded",command));break;
            case RoutingError::disabled:
                (void)enqueue(sender,make_status(*StatusCode::parse("125"),"Chat operation is disabled",command));break;
            case RoutingError::unsupported_type: case RoutingError::private_broadcast: case RoutingError::unsupported_group:
                (void)enqueue(sender,make_status(*StatusCode::parse("125"),"Command not implemented",command));break;
            case RoutingError::invalid_state: reject(sender,"244",command);break;
            case RoutingError::wrong_source: case RoutingError::invalid_message: reject(sender,"240",command);break;
            }
            return;
        }
        const auto& plan=std::get<RoutedMessage>(prepared);
        const auto result=message_router_.dispatch(plan,[&](users::SessionId recipient,const RoutedFrame& frame) {
            const auto found=peers_.find(recipient.value());
            if (found==peers_.end()) { return false; }
            try { return std::visit([&](const auto& packet) { return enqueue_shared(*found->second,packet); },frame); }
            catch (const std::bad_alloc&) {
                check_model(sessions_.force_close(recipient,users::CloseCause::internal_error,users::Clock::now()));
                return false;
            }
        });
        if (const auto* error=std::get_if<chat::Error>(&result)) {
            if (*error==chat::Error::recipient_unavailable) { unavailable(); }
            else {
                attempt.outcome(chat::Event::internal_error);
                check_model(sessions_.force_close(sender.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
            }
        } else {
            const auto& report = std::get<chat::DispatchReport>(result);
            attempt.dispatched(message.type == MessageType::broadcast, report.queued, report.unavailable, report.refused);
        }
    } catch (const std::bad_alloc&) {
        attempt.outcome(chat::Event::internal_error);
        // Preparation/diagnostic failure cannot publish content. Queue failures
        // are caught per recipient above; never retry a partially routed plan.
        check_model(sessions_.force_close(sender.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
    }
}
std::variant<chat::DispatchReport, HubMessageError> LoginHub::send_hub_message(
        std::string_view text, std::optional<users::SessionId> target) {
    auto prepared = hub_messages_.prepare(text, target);
    if (const auto* error = std::get_if<HubMessageError>(&prepared)) { return *error; }
    return hub_messages_.dispatch(std::get<HubDelivery>(prepared), [&](users::SessionId recipient, const SharedHubMessageFrame& frame) {
        const auto found = peers_.find(recipient.value());
        if (found == peers_.end()) { return false; }
        try { return enqueue_shared(*found->second, frame); }
        catch (const std::bad_alloc&) {
            check_model(sessions_.force_close(recipient,users::CloseCause::internal_error,users::Clock::now()));
            return false;
        }
    });
}
void LoginHub::route_status(Id id, const Message& message) {
    auto& sender = *peers_.at(id);
    try {
        auto prepared = status_router_.prepare(sender.lifecycle.id(), message);
        if (const auto* error = std::get_if<StatusRoutingError>(&prepared)) {
            if (*error == StatusRoutingError::recipient_unavailable) {
                (void)enqueue(sender, make_status(*StatusCode::parse("140"), "Status recipient unavailable"));
            } else if (*error == StatusRoutingError::wrong_source) { reject(sender, "240"); }
            else if (*error == StatusRoutingError::invalid_state) { reject(sender, "244"); }
            else { reject(sender, "240"); }
            return;
        }
        const auto& delivery = std::get<StatusDelivery>(prepared);
        if (message.type == MessageType::direct || message.type == MessageType::echo) {
            transfers::peer_report(transfer_statistics_, delivery.frame()->code().error());
        }
        const auto result = status_router_.dispatch(delivery, [&](users::SessionId recipient, const SharedStatusFrame& frame) {
            const auto found = peers_.find(recipient.value());
            if (found == peers_.end()) { return false; }
            try { return enqueue_shared(*found->second, frame); }
            catch (const std::bad_alloc&) {
                check_model(sessions_.force_close(recipient,users::CloseCause::internal_error,users::Clock::now()));
                return false;
            }
        });
        if (std::holds_alternative<StatusRoutingError>(result)) {
            check_model(sessions_.force_close(sender.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
        }
    } catch (const std::bad_alloc&) {
        check_model(sessions_.force_close(sender.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
    }
}

std::string_view LoginHub::output(Id id) const {
    const auto& peer = *peers_.at(id);
    return peer.output.empty() ? std::string_view{} : peer.output.front().wire().substr(peer.offset);
}
bool LoginHub::consume(Id id, std::size_t bytes) {
    auto& peer = *peers_.at(id);
    if (peer.output.empty() || bytes == 0 || bytes > peer.output.front().wire().size() - peer.offset) {
        throw std::logic_error("Invalid transport send completion");
    }
    peer.offset += bytes;
    if (peer.offset != peer.output.front().wire().size()) { return false; }
    auto frame = std::move(peer.output.front());
    peer.output.pop_front();
    peer.offset = 0;
    peer.queued -= frame.wire().size();
    queued_ -= frame.wire().size();
    if (peer.lifecycle.closing()) { return false; }
    ClientSessionResult observed;
    try { observed = peer.session.observe(Direction::to_client, frame.message()); }
    catch (const std::bad_alloc&) {
        if (frame.message().kind != Command::sta) { throw; }
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now())); return false;
    }
    if (observed) {
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now())); return false;
    }
    if (frame.completion == Completion::support) {
        if (const auto error = peer.session.prepare_sid()) { reject(peer, sid_status_code(*error)); return false; }
        (void)enqueue(peer, *peer.session.sid_reply());
    } else if (frame.completion == Completion::authentication) {
        if (!peer.authentication.sent()) {
            check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
        }
    } else if (frame.completion == Completion::welcome) {
        if (user_login_.complete_welcome(peer.lifecycle.id(), peer.session, users::Clock::now(), peer.verified_account)) {
            check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
            return false;
        }
        peer.verified_account.reset();
        return true;
    }
    return false;
}
void LoginHub::disconnect(Id id, bool announce, users::CloseCause cause, users::TimePoint now) {
    const auto found = peers_.find(id);
    if (found == peers_.end()) { return; }
    auto& peer = *found->second;
    const auto result = user_disconnect_.begin(peer.lifecycle.id(),cause,now);
    const auto* departure = std::get_if<Departure>(&result);
    if (!departure) { throw std::logic_error("User disconnect lifecycle rejected"); }
    (void)search_.forget_searches(peer.lifecycle.id());
    // Reclaim all source resources before any allocating public notification.
    // This also releases global queue capacity for the surviving peers' QUI.
    queued_ -= peer.queued;
    peer.session.disconnect();
    peers_.erase(found); // Drop protocol/output references before deleting the model.
    if (user_disconnect_.release(departure->id)) { throw std::logic_error("User disconnect cleanup failed"); }
    if (announce && departure->first) { announce_departure(*departure); }
}
void LoginHub::announce_departure(const Departure& departure) {
    const auto fail_recipient = [this](Peer& peer) {
        check_model(sessions_.force_close(peer.lifecycle.id(),users::CloseCause::internal_error,users::Clock::now()));
    };
    std::optional<Message> notice;
    if (departure.public_sid) {
        try { notice.emplace(make_quit(*departure.public_sid)); }
        catch (const std::bad_alloc&) {
            // A client that cannot learn this departure must not keep using a
            // stale list. Do not allocate another frame under memory pressure.
            for (auto& [id,peer] : peers_) {
                (void)id;
                if (public_member(peer->lifecycle)) { fail_recipient(*peer); }
            }
            return;
        }
    } else if (!departure.invalid_public_sid) { return; } // Anonymous connection.
    for (auto& [id,peer] : peers_) {
        (void)id;
        if (!public_member(peer->lifecycle)) { continue; }
        if (!notice) { fail_recipient(*peer); continue; }
        try { (void)enqueue(*peer,*notice); }
        catch (const std::bad_alloc&) { fail_recipient(*peer); }
    }
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
