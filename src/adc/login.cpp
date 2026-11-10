#include "dc24h/adc/login.hpp"
#include "dc24h/adc/serializer.hpp"

#include <deque>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dc24h::adc {
struct LoginHub::Peer {
    struct Frame { Message message; std::string wire; Completion completion; };
    Peer(std::shared_ptr<SidPool> pool, std::string_view address) : session(std::move(pool), address) {}
    ClientSession session;
    std::deque<Frame> output;
    std::size_t offset{0};
    std::size_t queued{0};
    bool admitted{false};
    bool ready{false};
    bool closing{false};
    bool abort{false};
    bool overloaded{false};
    std::string public_sid;
    std::string public_nickname;
    std::string public_cid;
};
namespace {
Message info_message(std::string command, Command kind, std::vector<std::string> parameters) {
    return {MessageType::info, std::move(command), kind, {}, {}, {}, std::move(parameters), {}};
}
std::string_view error_status(const ClientSessionError& error) {
    return std::visit([](auto value) -> std::string_view {
        using T = decltype(value);
        if constexpr (std::is_same_v<T, InfError>) { return inf_status_code(value); }
        else if constexpr (std::is_same_v<T, SupportError>) { return support_status_code(value); }
        else if constexpr (std::is_same_v<T, SidError>) { return sid_status_code(value); }
        else { return "240"; }
    }, error);
}
} // namespace

LoginHub::LoginHub(LoginLimits limits) : limits_(limits), pool_(std::make_shared<SidPool>()) {
    if (limits.connections == 0 || limits.connections > 65535 ||
        limits.output_per_client < 128 || limits.output_per_client > 16 * 1024 * 1024 ||
        limits.output_total < limits.output_per_client || limits.output_total > 256 * 1024 * 1024) {
        throw std::invalid_argument("Invalid login/output limits");
    }
}
LoginHub::~LoginHub() = default;

void LoginHub::connect(Id id, std::string_view address) {
    if (id == 0 || peers_.contains(id) || peers_.size() >= limits_.connections) {
        throw std::logic_error("Duplicate or excess login connection");
    }
    peers_.emplace(id, std::make_unique<Peer>(pool_, address));
}

bool LoginHub::enqueue(Peer& peer, Message message, Completion completion) {
    if (peer.abort) { return false; }
    auto encoded = serialize(message);
    auto* wire = std::get_if<std::string>(&encoded);
    if (!wire) { throw std::logic_error("Invalid locally generated ADC output"); }
    if (wire->size() > limits_.output_per_client - peer.queued || wire->size() > limits_.output_total - queued_) {
        peer.abort = true; // Owner closes immediately; do not disturb a pinned TLS front.
        peer.overloaded = true;
        return false;
    }
    const auto size = wire->size();
    peer.output.push_back({std::move(message), std::move(*wire), completion});
    peer.queued += size;
    queued_ += size;
    return true;
}
void LoginHub::reject(Peer& peer, std::string_view status) {
    if (peer.closing) { return; }
    peer.closing = true;
    (void)enqueue(peer, info_message("STA", Command::sta, {std::string(status), "Request rejected"}));
}
void LoginHub::relay(Id except, const Message& message) {
    for (auto& [id, peer] : peers_) {
        if (id != except && peer->admitted && !peer->closing && !peer->abort) { (void)enqueue(*peer, message); }
    }
}

void LoginHub::receive(Id id, const Message& message) {
    auto& peer = *peers_.at(id);
    if (peer.closing || peer.abort) { return; }
    const auto before = peer.session.state();
    const auto previous = message.kind == Command::inf ? peer.session.info() : std::optional<ClientInfo>{};
    if (before == ConnectionState::normal && !peer.ready) { reject(peer, "240"); return; }
    if (message.kind == Command::inf && message.type == MessageType::broadcast) {
        // Check prospective nickname before the session commits an update.
        for (const auto& token : message.parameters) {
            const auto field = parse_named_parameter(token);
            if (field && field->name == "NI") {
                const auto found = nicknames_.find(std::string(field->value));
                if (found != nicknames_.end() && found->second != id) { reject(peer, "222"); return; }
            }
        }
    }
    if (const auto error = peer.session.observe(Direction::from_client, message)) {
        reject(peer, error_status(*error));
        return;
    }
    if (peer.session.state() == ConnectionState::disconnected) { peer.abort = true; return; }
    if (before == ConnectionState::protocol && message.kind == Command::sup) {
        (void)enqueue(peer, *peer.session.support().initial_reply(), Completion::support);
    } else if (message.kind == Command::inf) {
        const auto& metadata = *peer.session.info();
        const std::string nickname(*metadata.field("NI"));
        const std::string cid(*metadata.field("ID"));
        const auto duplicate = cids_.find(cid);
        if (duplicate != cids_.end() && duplicate->second != id) { reject(peer, "224"); return; }
        if (before == ConnectionState::identify) {
            // Accounts are not implemented: this release explicitly admits guests.
            // CID proof is not an account/password/role authentication mechanism.
            if (peer.session.approve_identity(IdentityMode::guest)) { reject(peer, "240"); return; }
            if (!enqueue(peer, info_message("INF", Command::inf, {"CT32", "NIdc24h.eu", "DEGuest login alpha"}))) { return; }
            for (const auto& [other, existing] : peers_) {
                if (other != id && existing->admitted && !existing->closing && !existing->abort) {
                    if (!enqueue(peer, existing->session.info()->public_message(*Sid::parse(existing->session.sid())))) { return; }
                }
            }
            // Include pending admissions in snapshots and relays. FIFO ordering
            // preserves welcome-before-join and avoids simultaneous-login gaps.
            const auto public_info = metadata.public_message(*Sid::parse(peer.session.sid()));
            if (!enqueue(peer, public_info, Completion::welcome)) { return; } // Own INF is last in the initial list.
            peer.public_sid = peer.session.sid();
            peer.public_nickname = nickname;
            peer.public_cid = cid;
            nicknames_.emplace(nickname, id);
            cids_.emplace(cid, id);
            peer.admitted = true;
            relay(id, public_info);
        } else {
            nicknames_.erase(peer.public_nickname);
            nicknames_.emplace(nickname, id);
            peer.public_nickname = nickname;
            // A complete public snapshot also conveys optional field cancellations.
            // Relay the sanitized update below, preserving bare cancellation names.
            auto update = metadata.public_message(*Sid::parse(peer.session.sid()));
            for (const auto& token : previous->public_message(*Sid::parse(peer.public_sid)).parameters) {
                const auto field = *parse_named_parameter(token);
                if (!metadata.field(field.name)) { update.parameters.emplace_back(field.name); }
            }
            relay(0, update); // Transport IDs are nonzero; includes the sender.
        }
    } else if (before == ConnectionState::normal && message.kind != Command::sup && message.kind != Command::sta) {
        // Passing the state gate does not implement chat/search/transfers.
        (void)enqueue(peer, info_message("STA", Command::sta, {"125", "Command not implemented"}));
    }
}

std::string_view LoginHub::output(Id id) const {
    const auto& peer = *peers_.at(id);
    return peer.output.empty() ? std::string_view{} : std::string_view(peer.output.front().wire).substr(peer.offset);
}
void LoginHub::consume(Id id, std::size_t bytes) {
    auto& peer = *peers_.at(id);
    if (peer.output.empty() || bytes == 0 || bytes > peer.output.front().wire.size() - peer.offset) {
        throw std::logic_error("Invalid transport send completion");
    }
    peer.offset += bytes;
    if (peer.offset != peer.output.front().wire.size()) { return; }
    auto frame = std::move(peer.output.front());
    peer.output.pop_front();
    peer.offset = 0;
    peer.queued -= frame.wire.size();
    queued_ -= frame.wire.size();
    if (peer.closing || peer.abort) { return; }
    if (peer.session.observe(Direction::to_client, frame.message)) { peer.abort = true; return; }
    if (frame.completion == Completion::support) {
        if (const auto error = peer.session.prepare_sid()) { reject(peer, sid_status_code(*error)); return; }
        (void)enqueue(peer, *peer.session.sid_reply());
    } else if (frame.completion == Completion::welcome) { peer.ready = true; }
}
void LoginHub::remove_identity(Id id, const Peer& peer) {
    const auto nickname = nicknames_.find(peer.public_nickname);
    if (nickname != nicknames_.end() && nickname->second == id) { nicknames_.erase(nickname); }
    const auto cid = cids_.find(peer.public_cid);
    if (cid != cids_.end() && cid->second == id) { cids_.erase(cid); }
}
void LoginHub::disconnect(Id id, bool announce) {
    const auto found = peers_.find(id);
    if (found == peers_.end()) { return; }
    auto& peer = *found->second;
    if (announce && peer.admitted) { relay(id, info_message("QUI", Command::qui, {peer.public_sid})); }
    remove_identity(id, peer);
    queued_ -= peer.queued;
    peer.session.disconnect();
    peers_.erase(found); // Drop all old output before releasing/reusing the SID.
}
bool LoginHub::closing(Id id) const { return peers_.at(id)->closing; }
bool LoginHub::must_close(Id id) const {
    const auto& peer = *peers_.at(id);
    return peer.abort || (peer.closing && peer.output.empty());
}
bool LoginHub::ready(Id id) const {
    const auto& peer = *peers_.at(id);
    return peer.ready && !peer.closing && !peer.abort;
}
bool LoginHub::overloaded(Id id) const { return peers_.at(id)->overloaded; }
ConnectionState LoginHub::state(Id id) const { return peers_.at(id)->session.state(); }
} // namespace dc24h::adc
