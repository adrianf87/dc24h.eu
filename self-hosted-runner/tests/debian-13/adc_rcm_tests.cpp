#include "dc24h/adc/rcm_routing.hpp"
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
RoutedRcm prepared(RcmRouter& router, bool echo = false) {
    auto result = router.prepare(id(1), parse(std::string(echo ? "ERCM" : "DRCM") + " ABCD EFGH ADC/1.0 RcmPrivate\\stoken XXkeep XXmore"));
    require(std::holds_alternative<RoutedRcm>(result), "RCM preparation failed"); return std::get<RoutedRcm>(std::move(result));
}
void codec() {
    const std::string wire = "ERCM ABCD EFGH future/1.0 Żółw\\stoken\\\\x XXkeep XXmore TOextension";
    auto parsed = parse_rcm(wire); require(std::holds_alternative<RcmRequest>(parsed), "Valid RCM refused");
    auto owned = std::get<RcmRequest>(std::move(parsed));
    require(owned.protocol() == "future/1.0" && owned.token() == "Żółw token\\x" && std::get<std::string>(serialize(owned.message())) == wire+'\n', "RCM roundtrip/projection differs");
    auto native = owned.message(); auto copy = std::get<RcmRequest>(validate_rcm(native)); native.parameters[1] = "changed";
    require(copy.token() == owned.token(), "RCM borrowed input");
    for (const auto& text : {"", "DRCM ABCD EFGH", "DRCM ABCD EFGH ADC/1.0", "DRCM ABCD EFGH ADC/1.0 1515 token", "DRCM ABCD EFGH ADC/1.0 bad\\q"}) require(!std::holds_alternative<RcmRequest>(parse_rcm(text)), "Malformed RCM accepted");
    for (const auto& protocol : {std::string(33,'P'),std::string("bad\\sprotocol"),std::string("Żółw"),std::string("bad\\nprotocol")}) require(std::get<RcmError>(parse_rcm("DRCM ABCD EFGH "+protocol+" token")) == RcmError::invalid_protocol, "Protocol bound failed");
    for (const auto& token : {std::string(129,'T'),std::string("bad\\ntoken")}) require(std::get<RcmError>(parse_rcm("DRCM ABCD EFGH ADC/1.0 "+token)) == RcmError::invalid_token, "Token bound failed");
    for (const auto& type : {"BRCM ABCD","FRCM ABCD +TCP4","CRCM","HRCM","IRCM","URCM ABCDEFGH"}) require(std::get<RcmError>(parse_rcm(std::string(type)+" ADC/1.0 token")) == RcmError::invalid_type, "Unsupported scope accepted");
    native = parse("DRCM ABCD EFGH ADC/1.0 token"); native.positional_count = 3;
    require(std::get<RcmError>(validate_rcm(native)) == RcmError::invalid_message, "Wrong native layout accepted");
    native.positional_count = 2; native.parameters.push_back("bad");
    require(std::get<RcmError>(validate_rcm(native)) == RcmError::invalid_message, "Malformed extension accepted");
    native = parse("DRCM ABCD EFGH "+std::string(32,'P')+" "+std::string(128,'T'));
    native.parameters.push_back("XX"+std::string(max_rcm_bytes-32-128-2,'x'));
    require(std::holds_alternative<RcmRequest>(validate_rcm(native)), "Exact aggregate limit refused");
    native.parameters.back() += 'x'; fail_after=0; const auto oversized=validate_rcm(native); fail_after=-1;
    require(std::get<RcmError>(oversized)==RcmError::field_limit,"Hostile native payload copied before bound");
    native.parameters=std::vector<std::string>(max_parameters+1,"XX");
    require(std::get<RcmError>(validate_rcm(native))==RcmError::field_limit,"Unbounded parameter count");
}
void routing_and_lifetimes() {
    users::SessionManager sessions(8); add(sessions,1,"ABCD",profile({})); add(sessions,2,"EFGH"); add(sessions,3,"IJKL");
    transfers::Core core(sessions); RcmRouter router(core), foreign(core);
    require(core.resolve_target("EFGH") == id(2) && !core.resolve_target("ZZZZ"), "Ready target lookup differs");
    auto direct=prepared(router); auto copy=direct; unsigned calls=0;
    require(direct.plan().listener()==id(2) && direct.plan().connector()==id(1) && !direct.plan().request().port(), "Reverse roles fabricated a port");
    const RcmRouter::Sink sink=[&](auto target,const auto& frame){++calls;require(target==id(2) && frame==direct.frame(),"DRCM reached wrong recipient or copied frame");return true;};
    require(std::get<RcmRoutingError>(foreign.dispatch(direct,sink))==RcmRoutingError::foreign_plan && !foreign.cancel(direct),"Foreign router accepted plan");
    require(std::get<RcmRoutingError>(router.dispatch(direct,{}))==RcmRoutingError::missing_sink,"Empty sink consumed plan");
    require(std::get<RcmDispatchReport>(router.dispatch(direct,sink))==RcmDispatchReport{1,0,0} && calls==1,"DRCM dispatch differs");
    require(std::get<RcmRoutingError>(router.dispatch(copy,sink))==RcmRoutingError::already_dispatched && calls==1,"Copy replayed RCM");
    auto echo=prepared(router,true); std::vector<users::SessionId> recipients;
    require(std::get<RcmDispatchReport>(router.dispatch(echo,[&](auto target,const auto& frame){recipients.push_back(target);require(frame==echo.frame(),"ERCM copied immutable packet");return true;}))==RcmDispatchReport{2,0,0} && recipients==std::vector<users::SessionId>{id(2),id(1)},"ERCM target/echo differs");
    auto refused=prepared(router,true);require(std::get<RcmDispatchReport>(router.dispatch(refused,[&](auto target,const auto&){return target==id(1);}))==RcmDispatchReport{1,1,0},"Target refusal suppressed independent echo");
    auto canceled=prepared(router);require(router.cancel(canceled)&&!router.cancel(canceled)&&std::get<RcmRoutingError>(router.dispatch(canceled,sink))==RcmRoutingError::already_dispatched,"Cancellation replayed RCM");
    struct Failure{}; auto throwing=prepared(router,true);unsigned tries=0;
    try{(void)router.dispatch(throwing,[&](auto,const auto&)->bool{if(++tries==2){throw Failure{};}return true;});require(false,"Echo exception swallowed");}catch(const Failure&){}
    require(tries==2&&std::get<RcmRoutingError>(router.dispatch(throwing,sink))==RcmRoutingError::already_dispatched,"Partial echo failure allowed retry");
    auto source_active=prepared(router,true);
    require(std::get<RcmDispatchReport>(router.dispatch(source_active,[&](auto,const auto&){update(sessions,1,profile());return true;}))==RcmDispatchReport{1,0,1},"Source activation did not cancel echo");
    update(sessions,1,profile({}));auto stale=prepared(router);remove(sessions,2);add(sessions,4,"EFGH");
    require(std::get<RcmRoutingError>(router.dispatch(stale,[](auto,const auto&){return true;}))==RcmRoutingError::recipient_unavailable,"SID reuse retargeted RCM");
    auto closure=prepared(router,true);require(std::get<RcmDispatchReport>(router.dispatch(closure,[&](auto,const auto&){remove(sessions,1);return true;}))==RcmDispatchReport{1,0,1},"Closed source received echo");
}
void connectivity_and_faults() {
    users::SessionManager sessions(4);add(sessions,1,"ABCD",profile({}));add(sessions,2,"EFGH");transfers::Core core(sessions);RcmRouter router(core);
    auto stale=prepared(router);update(sessions,2,profile({"TCP4"},"127.0.0.2"));
    require(std::get<RcmRoutingError>(router.dispatch(stale,[](auto,const auto&){return true;}))==RcmRoutingError::incompatible_connectivity,"Changed listener address ignored");
    update(sessions,2,profile({"UDP4"}));
    require(std::get<RcmRoutingError>(router.prepare(id(1),parse("DRCM ABCD EFGH ADC/1.0 token")))==RcmRoutingError::incompatible_connectivity,"UDP-only target became TCP active");
    update(sessions,2,profile({"TCP4","TCP6"}));update(sessions,1,profile({"TCP4"}));auto mixed=prepared(router);
    require(mixed.plan().request().family()==transfers::AddressFamily::ipv6,"Per-family passivity was conflated");
    update(sessions,1,profile({"TCP4","TCP6"}));
    require(std::get<RcmRoutingError>(router.prepare(id(1),parse("DRCM ABCD EFGH ADC/1.0 token")))==RcmRoutingError::incompatible_connectivity,"Fully active sender used reverse");
    update(sessions,1,profile({}));update(sessions,2,profile({"TCP6"},"","::1"));auto v6=prepared(router);require(v6.plan().request().family()==transfers::AddressFamily::ipv6,"IPv6 reverse path absent");
    update(sessions,2,profile({"TCP4","TCP6"}));auto dual=prepared(router);require(dual.plan().request().family()==transfers::AddressFamily::ipv4,"Dual-stack preference differs");
    update(sessions,2,profile({"TCP6"}));require(std::get<RcmRoutingError>(router.dispatch(dual,[](auto,const auto&){return true;}))==RcmRoutingError::incompatible_connectivity,"Old plan silently changed family");
    update(sessions,2,profile());const auto message=parse("ERCM ABCD EFGH "+std::string(32,'P')+" "+std::string(128,'T'));
    bool success=false;unsigned faults=0;
    for(long n=0;n<64;++n){fail_after=n;try{auto result=router.prepare(id(1),message);fail_after=-1;require(std::holds_alternative<RoutedRcm>(result),"Fault prep changed validation");success=true;break;}catch(const std::bad_alloc&){fail_after=-1;++faults;require(sessions.size()==2&&sessions.find(id(1))->ready()&&sessions.find(id(2))->ready(),"Fault changed sessions");}}
    require(success&&faults>=5,"Allocation fault points not covered");
    auto result=router.prepare(id(1),message);auto noalloc=std::get<RoutedRcm>(std::move(result));const RcmRouter::Sink sink=[](auto,const auto&){return true;};
    fail_after=0;const auto report=router.dispatch(noalloc,sink);const auto target=core.resolve_target("EFGH");fail_after=-1;
    require(std::get<RcmDispatchReport>(report)==RcmDispatchReport{2,0,0}&&target==id(2),"Dispatch/lookup allocated");
}
std::string drain(LoginHub& hub,unsigned n){std::string out;while(!hub.output(n).empty()&&!hub.must_close(n)){const std::string frame(hub.output(n));out+=frame;hub.consume(n,frame.size());}return out;}
std::string login(LoginHub& hub,unsigned n,bool active=true){
    hub.connect(n,"127.0.0.1");hub.receive(n,parse("HSUP ADBASE ADTIGR"));hub.consume(n,hub.output(n).size());const std::string sid(hub.output(n).substr(5,4));hub.consume(n,hub.output(n).size());TigerDigest pid{};pid[0]=static_cast<unsigned char>(n);
    hub.receive(n,parse("BINF "+sid+" NIUser"+std::to_string(n)+" ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)+" I40.0.0.0"+(active?" SUTCP4":"")));(void)drain(hub,n);return sid;
}
void live_queues(){
    LoginHub hub;const auto a=login(hub,1,false),b=login(hub,2);(void)login(hub,3);(void)drain(hub,1);(void)drain(hub,2);
    const auto echo="ERCM "+a+" "+b+" ADC/1.0 RcmQueuePrivate";hub.receive(1,parse(echo));const auto pinned=hub.output(2).data();hub.consume(2,3);
    hub.receive(1,parse("DRCM "+a+" "+b+" ADCS/1.0 RcmSecondPrivate"));require(hub.output(2).data()==pinned+3,"RCM append moved pinned front");
    require(drain(hub,1)==echo+'\n'&&hub.output(3).empty()&&drain(hub,2).find("ADCS/1.0")!=std::string::npos,"RCM shared delivery leaked or disappeared");
    require(hub.chat_statistics()==chat::Statistics{}&&hub.search_statistics()==search::Statistics{},"RCM entered Chat/Search totals");
    const auto candidate=parse("DRCM "+a+" "+b+" ADC/1.0 "+std::string(128,'T'));fail_after=0;hub.receive(1,candidate);fail_after=-1;
    require(hub.must_close(1)&&!hub.must_close(2),"Preparation failure did not isolate source");
    LoginLimits limits;limits.output_per_client=512;LoginHub queues(limits);const auto x=login(queues,1,false),y=login(queues,2);(void)drain(queues,1);
    queues.receive(1,parse("DRCM "+x+" "+y+" ADC/1.0 token XX"+std::string(1000,'x')));require(queues.must_close(2)&&!queues.must_close(1),"RCM target cap closed wrong peer");
}
}
int main(){try{codec();routing_and_lifetimes();connectivity_and_faults();live_queues();std::cout<<"PASS: ADC RCM codec, reverse roles, lifetimes, queues and faults\n";}catch(const std::exception& e){fail_after=-1;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
