#include "dc24h/transfers/core.hpp"
#include "dc24h/search/connectivity.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long fail_after = -1; }
void* operator new(std::size_t size) {
    if (fail_after == 0) { throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) { return memory; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using namespace dc24h;
using namespace dc24h::transfers;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
users::Profile profile(std::vector<std::string> features = {"TCP4"}, std::string v4 = "127.0.0.1", std::string v6 = "::1") {
    users::ProfileData data; data.features = std::move(features); data.ipv4 = std::move(v4); data.ipv6 = std::move(v6);
    const auto checked = users::Profile::create(std::move(data)); require(checked.has_value(), "Invalid fixture profile"); return *checked;
}
void add(users::SessionManager& sessions, std::uint64_t value, users::Profile data = profile(), int phase = 3, std::string sid = {}) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(value), value % 2 ? users::Transport::adc : users::Transport::adcs, {})), "Add failed");
    if (!phase) { return; }
    require(!sessions.start_negotiation(id(value), {}), "Negotiation failed");
    if (phase == 1) { return; }
    require(!sessions.identify(id(value), *users::User::create(*users::Identity::create(sid.empty() ? "SID" + std::to_string(value) : sid,
        "CID" + std::to_string(value), "Nick" + std::to_string(value)), std::move(data)), {}), "Identify failed");
    if (phase == 3) { require(!sessions.mark_ready(id(value), {}), "Ready failed"); }
}
void update(users::SessionManager& sessions, std::uint64_t value, users::Profile data) {
    require(!sessions.update_user(id(value), *users::User::create(*sessions.find(id(value))->identity(), std::move(data)), {}), "Update failed");
}
void remove(users::SessionManager& sessions, std::uint64_t value) {
    require(!sessions.finish_close(id(value), users::CloseCause::peer_closed, {}) && !sessions.remove(id(value)), "Remove failed");
}
RequestData request(Kind kind = Kind::connect_to_me, AddressFamily family = AddressFamily::ipv4) {
    return {kind, family, "ADC/1.0", kind == Kind::connect_to_me ? std::optional<std::int64_t>{1515} : std::nullopt, "TransferPrivate token"};
}
Plan prepare(const Core& core, RequestData data = request()) {
    auto result = core.prepare(id(1), "SID1", "SID2", data);
    require(std::holds_alternative<Plan>(result), "Preparation failed"); return std::get<Plan>(std::move(result));
}
void validation() {
    auto data = request();
    auto checked = Request::create(data); auto owned = std::get<Request>(std::move(checked));
    data.protocol = "mutated"; data.token = "changed"; data.port = 2;
    require(owned.protocol() == "ADC/1.0" && owned.token() == "TransferPrivate token" && owned.port() == 1515, "Request borrowed input");
    for (const auto port : {-1LL, 0LL, 65536LL, 9223372036854775807LL}) {
        data = request(); data.port = port;
        require(std::get<RequestError>(Request::create(data)) == RequestError::invalid_port, "Port overflow accepted");
    }
    for (const auto port : {1, 65535}) { data = request(); data.port = port; require(std::holds_alternative<Request>(Request::create(data)), "Legal port refused"); }
    data = request(); data.port.reset(); require(std::get<RequestError>(Request::create(data)) == RequestError::invalid_port, "Missing port accepted");
    data = request(Kind::reverse_connect); require(std::holds_alternative<Request>(Request::create(data)), "Reverse without port refused");
    data.port = 1; require(std::get<RequestError>(Request::create(data)) == RequestError::invalid_port, "Reverse port accepted");
    for (bool protocol : {false, true}) {
        const auto max = protocol ? max_protocol_bytes : max_token_bytes;
        for (const auto& text : std::vector<std::string>{"", "bad\n", "bad\r", "bad\t", std::string("a\0b", 3), "e\xcc\x81", "\xff", std::string(max + 1, 'x')}) {
            data = request(); (protocol ? data.protocol : data.token) = text;
            require(std::get<RequestError>(Request::create(data)) == (protocol ? RequestError::invalid_protocol : RequestError::invalid_token), "Invalid opaque text accepted");
        }
        data = request(); (protocol ? data.protocol : data.token) = std::string(max, '~');
        require(std::holds_alternative<Request>(Request::create(data)), "Exact text bound refused");
    }
    data = request(); data.protocol = "Unknown protocol"; data.token = "Żółw \\ token";
    require(std::get<Request>(Request::create(data)).token() == data.token, "Unknown protocol/NFC token rewritten");
    data.kind = static_cast<Kind>(99); require(std::get<RequestError>(Request::create(data)) == RequestError::invalid_kind, "Invalid kind accepted");
    data = request(); data.family = static_cast<AddressFamily>(99); require(std::get<RequestError>(Request::create(data)) == RequestError::invalid_family, "Invalid family accepted");
}
void readiness_and_paths() {
    users::SessionManager sessions(12); Core core(sessions);
    require(std::get<Error>(core.prepare(id(1), "SID1", "SID2", {})) == Error::missing_sender, "Missing sender accepted");
    add(sessions, 1, profile(), 0);
    for (int phase = 0; phase < 3; ++phase) {
        require(std::get<Error>(core.prepare(id(1), "SID1", "SID2", {})) == Error::sender_not_ready, "Unready sender accepted");
        if (!phase) { require(!sessions.start_negotiation(id(1), {}), "Start failed"); }
        if (phase == 1) { require(!sessions.identify(id(1), *users::User::create(*users::Identity::create("SID1", "CID1", "Nick1"), profile()), {}), "Identify failed"); }
    }
    require(!sessions.mark_ready(id(1), {}), "Ready failed");
    require(std::get<Error>(core.prepare(id(1), "SID2", "missing", {})) == Error::wrong_source, "Source priority changed");
    require(std::get<Error>(core.prepare(id(1), "SID1", "SID1", {})) == Error::self_connection, "Self connection accepted");
    require(std::get<Error>(core.prepare(id(1), "SID1", "missing", {})) == Error::recipient_unavailable, "Missing target accepted");
    add(sessions, 2, profile({}), 2);
    require(std::get<Error>(core.prepare(id(1), "SID1", "SID2", request())) == Error::recipient_unavailable, "Welcome-pending target accepted");
    require(!sessions.mark_ready(id(2), {}), "Target ready failed");
    auto connect = prepare(core);
    require(connect.listener() == id(1) && connect.connector() == id(2) && connect.target() == id(2) && connect.listener_address()[0] == 127, "Connect roles/address differ");
    require(std::get<Error>(core.prepare(id(1), "SID1", "SID2", request(Kind::reverse_connect))) == Error::incompatible_connectivity, "Active requester reverse accepted");
    update(sessions, 1, profile({"UDP4"}));
    require(std::get<Error>(core.prepare(id(1), "SID1", "SID2", request())) == Error::incompatible_connectivity, "UDP activity became TCP");
    require(std::get<Error>(core.prepare(id(1), "SID1", "SID2", request(Kind::reverse_connect))) == Error::incompatible_connectivity, "Passive/passive accepted");
    update(sessions, 2, profile({"TCP4", "TCP6"}));
    for (const auto family : {AddressFamily::ipv4, AddressFamily::ipv6}) {
        auto reverse = prepare(core, request(Kind::reverse_connect, family));
        require(reverse.listener() == id(2) && reverse.connector() == id(1) && !reverse.request().port(), "Reverse roles invented port");
        update(sessions, 1, profile({"TCP4", "TCP6"}));
        auto direct = prepare(core, request(Kind::connect_to_me, family));
        require(direct.listener() == id(1), "Family selected wrong listener");
        update(sessions, 1, profile({"UDP4"}));
    }
}
void dispatch_lifecycle() {
    users::SessionManager sessions(8); add(sessions, 1); add(sessions, 2, profile({})); add(sessions, 3);
    Core core(sessions), other(sessions); auto plan = prepare(core); auto copy = plan;
    unsigned calls = 0;
    const Core::Sink sink = [&](users::SessionId target, const Plan& value) {
        ++calls; require(target == id(2) && value.state() == State::dispatching, "Wrong target/state");
        require(!core.cancel(value) && std::get<Error>(core.dispatch(value, [](auto, const auto&) { return true; })) == Error::already_consumed, "Reentrant duplicate accepted");
        return true;
    };
    require(std::get<Error>(other.dispatch(plan, sink)) == Error::foreign_plan && !other.cancel(plan), "Foreign plan accepted");
    require(std::get<Error>(core.dispatch(plan, {})) == Error::missing_sink && plan.state() == State::prepared, "Missing sink consumed plan");
    require(!sessions.rename(id(1), "Renamed", {}), "Rename failed");
    require(std::get<State>(core.dispatch(plan, sink)) == State::queued && copy.state() == State::queued && calls == 1, "One-attempt copy failed");
    require(std::get<Error>(core.dispatch(copy, sink)) == Error::already_consumed && calls == 1, "Copy repeated dispatch");
    auto refused = prepare(core); require(std::get<State>(core.dispatch(refused, [](auto, const auto&) { return false; })) == State::refused, "Queue refusal lost");
    auto replaced = prepare(core); auto original = replaced; auto replacement = prepare(core);
    require(std::get<State>(core.dispatch(replaced, [&](auto, const auto&) { replaced = replacement; return true; })) == State::queued &&
        original.state() == State::queued && replaced.state() == State::prepared, "Sink replaced the consumed attempt");
    auto moved = std::move(replaced);
    require(replaced.state() == State::canceled && !core.owns(replaced) && core.owns(moved), "Moved-from plan remained dispatchable");
    auto canceled = prepare(core); auto canceled_copy = canceled;
    require(core.cancel(canceled) && !core.cancel(canceled_copy) && std::get<Error>(core.dispatch(canceled_copy, sink)) == Error::already_consumed, "Cancel copy dispatched");
    auto throwing = prepare(core); struct Failure {};
    try { (void)core.dispatch(throwing, [](auto, const auto&) -> bool { throw Failure{}; }); require(false, "Exception swallowed"); }
    catch (const Failure&) {}
    require(throwing.state() == State::failed && std::get<Error>(core.dispatch(throwing, sink)) == Error::already_consumed, "Throw allowed retry");
    auto target_gone = prepare(core); remove(sessions, 2); add(sessions, 4, profile({}), 3, "SID2");
    require(std::get<Error>(core.dispatch(target_gone, sink)) == Error::recipient_unavailable && target_gone.state() == State::canceled, "SID reuse retargeted plan");
    auto source_gone = prepare(core); remove(sessions, 1); add(sessions, 5, profile(), 3, "SID1");
    require(std::get<Error>(core.dispatch(source_gone, sink)) == Error::missing_sender, "Source SID reuse accepted");
}
void dynamic_connectivity() {
    users::SessionManager sessions(4); add(sessions, 1); add(sessions, 2, profile({})); Core core(sessions);
    auto original = prepare(core); update(sessions, 1, profile({"UDP4"}));
    require(std::get<Error>(core.dispatch(original, [](auto, const auto&) { return true; })) == Error::incompatible_connectivity && original.state() == State::canceled, "TCP revocation ignored");
    update(sessions, 1, profile()); auto changed = prepare(core); update(sessions, 1, profile({"TCP4"}, "127.0.0.2"));
    require(std::get<Error>(core.dispatch(changed, [](auto, const auto&) { return true; })) == Error::incompatible_connectivity, "Address update silently retargeted");
    require(changed.listener_address()[3] == 1, "Old plan address mutated");
    auto fresh = prepare(core); require(fresh.listener_address()[3] == 2, "New plan missed address update");
    update(sessions, 1, profile({"TCP4", "UDP6"}, "127.0.0.2"));
    require(std::get<State>(core.dispatch(fresh, [](auto, const auto&) { return true; })) == State::queued, "Unrelated UDP change canceled TCP");
    update(sessions, 1, profile({})); update(sessions, 2, profile()); auto reverse = prepare(core, request(Kind::reverse_connect));
    update(sessions, 1, profile());
    require(std::get<Error>(core.dispatch(reverse, [](auto, const auto&) { return true; })) == Error::incompatible_connectivity, "Reverse changed direction after source active");
    auto closure = prepare(core);
    require(std::get<State>(core.dispatch(closure, [&](auto, const auto&) { remove(sessions, 1); remove(sessions, 2); return true; })) == State::queued, "Sink lifetime mutation broke dispatch");
}
void shared_address_policy() {
    for (const auto& invalid : {"0.0.0.0", "255.255.255.255", "224.0.0.1"}) {
        const auto p = profile({"TCP4", "UDP4"}, invalid);
        require(!users::public_address(p, AddressFamily::ipv4) && !users::tcp_active(p, AddressFamily::ipv4) && !search::reply_paths(p).tcp4, "Unsafe IPv4 admitted");
    }
    for (const auto& invalid : {"::", "ff02::1", "fe80::1", "::ffff:127.0.0.1"}) {
        const auto p = profile({"TCP6", "UDP6"}, "127.0.0.1", invalid);
        require(!users::public_address(p, AddressFamily::ipv6) && !users::tcp_active(p, AddressFamily::ipv6) && !search::reply_paths(p).tcp6, "Unsafe IPv6 admitted");
    }
    const auto p = profile({"TCP4", "TCP6"});
    require(users::tcp_active(p, AddressFamily::ipv4) && users::tcp_active(p, AddressFamily::ipv6), "Dual-stack activity lost");
    require(!users::public_address(p, static_cast<AddressFamily>(99)), "Invalid family admitted");
    require(!users::tcp_active(profile({"TCP0", "UDP4"}), AddressFamily::ipv4), "Wrong feature became active");
}
void faults_and_domain() {
    users::SessionManager sessions(4); add(sessions, 1); add(sessions, 2, profile({})); Core core(sessions);
    auto data = request(); data.protocol = std::string(max_protocol_bytes, 'P'); data.token = std::string(max_token_bytes, 'T');
    unsigned failures = 0; bool succeeded = false;
    for (long budget = 0; budget < 16; ++budget) {
        fail_after = budget;
        try { auto result = core.prepare(id(1), "SID1", "SID2", data); fail_after = -1; require(std::holds_alternative<Plan>(result), "Fault preparation changed validation"); succeeded = true; break; }
        catch (const std::bad_alloc&) { fail_after = -1; ++failures; require(sessions.size() == 2 && sessions.find(id(1))->ready() && sessions.find(id(2))->ready(), "Failure mutated sessions"); }
    }
    require(succeeded && failures >= 3, "Preparation allocation points not exercised");
    auto noalloc = prepare(core); auto cancel = prepare(core);
    const Core::Sink sink = [](auto, const auto&) { return true; };
    fail_after = 0;
    const auto outcome = core.dispatch(noalloc, sink); const bool canceled = core.cancel(cancel);
    const auto address = users::public_address(sessions.find(id(1))->user()->profile(), AddressFamily::ipv4);
    fail_after = -1;
    require(std::get<State>(outcome) == State::queued && canceled && address, "Dispatch/cancel/address allocated");
    alignas(Core) unsigned char storage[sizeof(Core)]; auto* first = new (storage) Core(sessions);
    auto old = prepare(*first); first->~Core(); auto* replacement = new (storage) Core(sessions);
    const bool refused = !replacement->owns(old) && std::get<Error>(replacement->dispatch(old, sink)) == Error::foreign_plan;
    replacement->~Core(); require(refused, "Core address reuse admitted old domain");
}
}
int main() {
    try { validation(); readiness_and_paths(); dispatch_lifecycle(); dynamic_connectivity(); shared_address_policy(); faults_and_domain();
        std::cout << "PASS: Transfer Core validation, roles, lifetimes, one-attempt dispatch, connectivity and allocation faults\n"; }
    catch (const std::exception& error) { fail_after = -1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
