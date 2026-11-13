#include "dc24h/adc/ctm_routing.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/search/connectivity.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long fail_after = -1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_after == 0) { throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) { return memory; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* memory) noexcept { std::free(memory); }
[[gnu::noinline]] void operator delete[](void* memory) noexcept { std::free(memory); }
[[gnu::noinline]] void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
[[gnu::noinline]] void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using namespace dc24h;
using namespace dc24h::adc;
void require(bool value, const char* why) { if (!value) { throw std::runtime_error(why); } }
Message parse(std::string_view line) { return std::get<Message>(parse_line(line)); }
users::SessionId id(std::uint64_t n) { return *users::SessionId::from_value(n); }
users::Profile profile(std::vector<std::string> features = {"TCP4"}, std::string v4 = "127.0.0.1", std::string v6 = "::1") {
    users::ProfileData data; data.features = std::move(features); data.ipv4 = std::move(v4); data.ipv6 = std::move(v6);
    return *users::Profile::create(std::move(data));
}
void add(users::SessionManager& sessions, unsigned n, std::string sid, users::Profile p = profile()) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n), users::Transport::adc, {})), "Add failed");
    require(!sessions.start_negotiation(id(n), {}), "Start failed");
    require(!sessions.identify(id(n), *users::User::create(*users::Identity::create(sid, "CID"+std::to_string(n), "Nick"+std::to_string(n)), std::move(p)), {}), "Identify failed");
    require(!sessions.mark_ready(id(n), {}), "Ready failed");
}
void update(users::SessionManager& s, unsigned n, users::Profile p) {
    require(!s.update_user(id(n), *users::User::create(*s.find(id(n))->identity(), std::move(p)), {}), "Update failed");
}
void remove(users::SessionManager& s, unsigned n) { require(!s.finish_close(id(n), users::CloseCause::peer_closed, {}) && !s.remove(id(n)), "Remove failed"); }
RoutedCtm prepared(CtmRouter& router, bool echo = false, std::string token = "CtmPrivate token") {
    const auto wire = std::string(echo ? "ECTM" : "DCTM") + " ABCD EFGH ADC/1.0 01515 "+std::string(token == "CtmPrivate token" ? "CtmPrivate\\stoken" : token)+" XXkeep XXmore";
    auto result = router.prepare(id(1), parse(wire)); require(std::holds_alternative<RoutedCtm>(result), "Router prepare failed"); return std::get<RoutedCtm>(std::move(result));
}
void codec() {
    const std::string wire = "ECTM ABCD EFGH future/1.0 00065535 Żółw\\stoken\\\\x XXkeep XXmore TOextension";
    auto result = parse_ctm(wire); require(std::holds_alternative<CtmRequest>(result), "Valid CTM rejected");
    auto owned = std::get<CtmRequest>(std::move(result));
    require(owned.port() == 65535 && owned.protocol() == "future/1.0" && owned.token() == "Żółw token\\x" && std::get<std::string>(serialize(owned.message())) == wire+'\n', "CTM projection/roundtrip differs");
    auto input = owned.message(); auto copy = std::get<CtmRequest>(validate_ctm(input)); input.parameters[2] = "changed";
    require(copy.token() == owned.token(), "CTM borrowed mutable fields");
    for (const auto& text : {"", "DCTM ABCD EFGH", "DCTM ABCD EFGH ADC/1.0 bad token", "DCTM ABCD EFGH ADC/1.0 9223372036854775808 token", "DCTM ABCD EFGH ADC/1.0 1 bad\\q"}) {
        const auto parsed = parse_ctm(text); require(!std::holds_alternative<CtmRequest>(parsed), "Raw malformed CTM accepted");
    }
    for (const auto& port : {"0", "-0", "-1", "65536"}) require(std::get<CtmError>(parse_ctm(std::string("DCTM ABCD EFGH ADC/1.0 ")+port+" token")) == CtmError::invalid_port, "Port semantic bound failed");
    for (const auto& protocol : {std::string(33,'P'), std::string("bad\\sprotocol"), std::string("Żółw"), std::string("bad\\nprotocol")}) {
        require(std::get<CtmError>(parse_ctm("DCTM ABCD EFGH "+protocol+" 1 token")) == CtmError::invalid_protocol, "Protocol wire grammar weakened");
    }
    for (const auto& token : {std::string(129,'T'), std::string("bad\\ntoken")}) require(std::get<CtmError>(parse_ctm("DCTM ABCD EFGH ADC/1.0 1 "+token)) == CtmError::invalid_token, "Token semantic bound failed");
    for (const auto& type : {"BCTM ABCD", "FCTM ABCD +TCP4", "CCTM", "HCTM", "ICTM", "UCTM ABCDEFGH"}) require(std::get<CtmError>(parse_ctm(std::string(type)+" ADC/1.0 1 token")) == CtmError::invalid_type, "Non-directed CTM accepted");
    auto native = parse("DCTM ABCD EFGH ADC/1.0 1 token"); native.positional_count = 2;
    require(std::get<CtmError>(validate_ctm(native)) == CtmError::invalid_message, "Forged layout accepted");
    native = parse("DCTM ABCD EFGH ADC/1.0 1 token"); native.parameters.push_back("bad");
    require(std::get<CtmError>(validate_ctm(native)) == CtmError::invalid_message, "Malformed extension accepted");
    native = parse("DCTM ABCD EFGH "+std::string(32,'P')+" 1 "+std::string(128,'T'));
    std::size_t bytes = 0; for (const auto& p : native.parameters) bytes += p.size();
    native.parameters.push_back("XX"+std::string(max_ctm_bytes-bytes-2,'x'));
    require(std::holds_alternative<CtmRequest>(validate_ctm(native)), "Exact aggregate limit rejected");
    native.parameters.back() += 'x'; fail_after = 0; auto oversized = validate_ctm(native); fail_after = -1;
    require(std::get<CtmError>(oversized) == CtmError::field_limit, "Aggregate check copied hostile object");
    native.parameters = std::vector<std::string>(max_parameters+1,"XX");
    require(std::get<CtmError>(validate_ctm(native)) == CtmError::field_limit, "Field count unlimited");
}
void routing() {
    users::SessionManager sessions(8); add(sessions, 1, "ABCD"); add(sessions, 2, "EFGH", profile({})); add(sessions, 3, "IJKL");
    transfers::Core core(sessions); CtmRouter router(core), foreign(core);
    auto direct = prepared(router); auto copied = direct; unsigned calls = 0; const CtmFrame* shared = nullptr;
    const CtmRouter::Sink sink = [&](users::SessionId target, const SharedCtmFrame& frame) { ++calls; require(target == id(2), "DCTM leaked to another recipient"); shared = frame.get(); return true; };
    require(std::get<CtmRoutingError>(foreign.dispatch(direct, sink)) == CtmRoutingError::foreign_plan && !foreign.cancel(direct), "Foreign router accepted plan");
    require(std::get<CtmRoutingError>(router.dispatch(direct, {})) == CtmRoutingError::missing_sink, "Missing sink consumed plan");
    require(std::get<CtmDispatchReport>(router.dispatch(direct, sink)) == CtmDispatchReport{1,0,0} && shared == direct.frame().get(), "DCTM delivery differs");
    require(std::get<CtmRoutingError>(router.dispatch(copied, sink)) == CtmRoutingError::already_dispatched && calls == 1, "Copy repeated CTM");
    auto echo = prepared(router, true); std::vector<users::SessionId> recipients;
    require(std::get<CtmDispatchReport>(router.dispatch(echo, [&](auto target, const auto& frame) { recipients.push_back(target); require(frame == echo.frame(), "Echo copied packet"); return true; })) == CtmDispatchReport{2,0,0} && recipients == std::vector<users::SessionId>{id(2),id(1)}, "ECTM target/echo order differs");
    auto refused = prepared(router, true);
    require(std::get<CtmDispatchReport>(router.dispatch(refused, [&](auto target, const auto&) { return target == id(1); })) == CtmDispatchReport{1,1,0}, "Queue refusal lost independent echo");
    auto canceled = prepared(router); require(router.cancel(canceled) && !router.cancel(canceled) && std::get<CtmRoutingError>(router.dispatch(canceled, sink)) == CtmRoutingError::already_dispatched, "Cancel/replay failed");
    auto throwing = prepared(router); struct Failure {};
    try { (void)router.dispatch(throwing, [](auto, const auto&) -> bool { throw Failure{}; }); require(false,"Throw swallowed"); } catch (const Failure&) {}
    require(throwing.plan().state() == transfers::State::failed && std::get<CtmRoutingError>(router.dispatch(throwing,sink)) == CtmRoutingError::already_dispatched,"Exception allowed retry");
    auto stale = prepared(router); remove(sessions,2); add(sessions,4,"EFGH",profile({}));
    require(std::get<CtmRoutingError>(router.dispatch(stale,sink)) == CtmRoutingError::recipient_unavailable, "SID reuse retargeted CTM");
    auto closing = prepared(router,true);
    require(std::get<CtmDispatchReport>(router.dispatch(closing,[&](auto, const auto&) { remove(sessions,1); return true; })) == CtmDispatchReport{1,0,1},"Source closure still received echo");
}
void connectivity_and_faults() {
    users::SessionManager sessions(4); add(sessions,1,"ABCD"); add(sessions,2,"EFGH",profile({})); transfers::Core core(sessions); CtmRouter router(core);
    auto stale = prepared(router); update(sessions,1,profile({"TCP4"},"127.0.0.2"));
    require(std::get<CtmRoutingError>(router.dispatch(stale,[](auto,const auto&){return true;})) == CtmRoutingError::inactive_sender,"Listener replacement ignored");
    update(sessions,1,profile({"UDP4"}));
    require(std::get<CtmRoutingError>(router.prepare(id(1),parse("DCTM ABCD EFGH ADC/1.0 1 token"))) == CtmRoutingError::inactive_sender,"UDP became TCP");
    update(sessions,1,profile({"TCP6"},"","::1")); auto v6 = prepared(router);
    require(v6.plan().request().family() == transfers::AddressFamily::ipv6,"IPv6 listener not selected");
    update(sessions,1,profile({"TCP4","TCP6"})); auto dual = prepared(router);
    require(dual.plan().request().family() == transfers::AddressFamily::ipv4,"Dual-stack preference unstable");
    update(sessions,1,profile({"TCP6"})); require(std::get<CtmRoutingError>(router.dispatch(dual,[](auto,const auto&){return true;})) == CtmRoutingError::inactive_sender,"Old plan silently changed family");
    update(sessions,1,profile());
    const auto frame = parse("ECTM ABCD EFGH "+std::string(32,'P')+" 1234 "+std::string(128,'T'));
    bool complete = false; unsigned failures = 0;
    for (long n = 0; n < 64; ++n) {
        fail_after = n;
        try { auto result = router.prepare(id(1),frame); fail_after = -1; require(std::holds_alternative<RoutedCtm>(result),"Fault preparation altered validation"); complete = true; break; }
        catch (const std::bad_alloc&) { fail_after = -1; ++failures; require(sessions.size() == 2 && sessions.find(id(1))->ready(),"Preparation failure mutated sessions"); }
    }
    require(complete && failures >= 5,"Allocation fault paths not exercised");
    auto result = router.prepare(id(1),frame); auto noalloc = std::get<RoutedCtm>(std::move(result));
    const CtmRouter::Sink sink = [](auto,const auto&){return true;};
    fail_after = 0; const auto report = router.dispatch(noalloc,sink); fail_after = -1;
    require(std::get<CtmDispatchReport>(report) == CtmDispatchReport{2,0,0},"Dispatch allocated or echo failed");
}
std::string drain(LoginHub& hub, unsigned n) { std::string out; while (!hub.output(n).empty() && !hub.must_close(n)) { const std::string frame(hub.output(n)); out += frame; hub.consume(n,frame.size()); } return out; }
std::string login(LoginHub& hub,unsigned n) {
    hub.connect(n,"127.0.0.1"); hub.receive(n,parse("HSUP ADBASE ADTIGR")); hub.consume(n,hub.output(n).size());
    const std::string sid(hub.output(n).substr(5,4)); hub.consume(n,hub.output(n).size()); TigerDigest pid{};pid[0]=static_cast<unsigned char>(n);
    hub.receive(n,parse("BINF "+sid+" NIUser"+std::to_string(n)+" ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)+" I40.0.0.0 SUTCP4")); (void)drain(hub,n); return sid;
}
void live_queues() {
    LoginHub hub; const auto a=login(hub,1), b=login(hub,2); (void)login(hub,3); (void)drain(hub,1);(void)drain(hub,2);
    const auto echo="ECTM "+a+" "+b+" ADC/1.0 1515 QueuedPrivate"; hub.receive(1,parse(echo));
    const auto pinned=hub.output(2).data(); hub.consume(2,3); require(hub.output(2).data()==pinned+3,"CTM partial front moved");
    hub.receive(1,parse("DCTM "+a+" "+b+" ADCS/1.0 1516 SecondPrivate"));
    require(hub.output(2).data()==pinned+3,"CTM append moved pinned output");
    require(drain(hub,1)==echo+'\n' && hub.output(3).empty(),"ECTM leaked or echo absent");
    require(drain(hub,2).find("ADCS/1.0")!=std::string::npos,"Queued second CTM disappeared");
    require(hub.chat_statistics()==chat::Statistics{} && hub.search_statistics()==search::Statistics{},"CTM entered chat/search counters");
    const auto fault=parse("DCTM "+a+" "+b+" ADC/1.0 1515 "+std::string(128,'T'));
    fail_after=0;hub.receive(1,fault);fail_after=-1;require(hub.must_close(1)&&!hub.must_close(2),"Preparation fault did not isolate source");
    LoginLimits limits;limits.output_per_client=512;LoginHub queues(limits);const auto x=login(queues,1),y=login(queues,2);(void)drain(queues,1);
    queues.receive(1,parse("DCTM "+x+" "+y+" ADC/1.0 1515 token XX"+std::string(1000,'x')));
    require(queues.must_close(2)&&!queues.must_close(1),"CTM recipient output cap affected wrong peer");
}
}
int main() { try { codec();routing();connectivity_and_faults();live_queues();std::cout<<"PASS: ADC CTM codec, routing, lifetimes, queues and faults\n"; }
catch(const std::exception& e){fail_after=-1;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;} }
