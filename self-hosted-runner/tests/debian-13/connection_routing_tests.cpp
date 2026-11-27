#include "dc24h/adc/connection_routing.hpp"
#include "dc24h/adc/ctm_routing.hpp"
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
using RoutingError = ConnectionRoutingError;
RoutedConnection prepared(ConnectionRouter& router, unsigned sender, std::string source, std::string target, bool reverse=false, bool echo=false, std::string token="SharedPrivateToken") {
    const auto line=std::string(echo?"E":"D")+(reverse?"RCM":"CTM")+" "+source+" "+target+" ADC/1.0 "+(reverse?"":"01515 ")+token+" XXprivate";
    auto result=router.prepare(id(sender),parse(line));require(std::holds_alternative<RoutedConnection>(result),"Connection preparation failed");return std::get<RoutedConnection>(std::move(result));
}
void sid_matrix() {
    users::SessionManager sessions(8);const std::array<std::string,4> keys{"AAAA","BBBB","CCCC","DDDD"};
    for(unsigned n=0;n<4;++n)add(sessions,n+1,keys[n]);
    transfers::Core core(sessions);ConnectionRouter router(core);
    for(bool reverse:{false,true})for(bool echo:{false,true})for(unsigned source=0;source<4;++source)for(unsigned target=0;target<4;++target){
        if(source==target)continue;
        for(unsigned n=0;n<4;++n)update(sessions,n+1,profile(reverse&&n==source?std::vector<std::string>{}:std::vector<std::string>{"TCP4"}));
        auto route=prepared(router,source+1,keys[source],keys[target],reverse,echo);const auto recipients=route.recipients();
        require(recipients.size()==(echo?2U:1U)&&recipients[0]==id(target+1)&&(!echo||recipients[1]==id(source+1)),"SID audience differs");
        require(route.plan().source()==keys[source]&&route.plan().target_key()==keys[target],"Public SID key changed");
        std::array<unsigned,4> counts{};
        const auto result=router.dispatch(route,[&](auto recipient,const auto& frame){++counts[recipient.value()-1];require(frame==route.frame(),"Per-recipient frame copied");return true;});
        require(std::get<ConnectionDispatchReport>(result)==ConnectionDispatchReport{echo?2U:1U,0,0},"Dispatch report differs");
        for(unsigned n=0;n<4;++n)require(counts[n]==(n==target||(echo&&n==source)?1U:0U),"Bystander received connection message");
    }
}
void admission_and_domains() {
    users::SessionManager sessions(8);add(sessions,1,"AAAA");add(sessions,2,"BBBB",profile({}));add(sessions,3,"CCCC",profile({}));add(sessions,4,"DDDD");
    transfers::Core core(sessions);ConnectionRouter router(core),foreign(core);CtmRouter legacy_ctm(core);RcmRouter legacy_rcm(core);
    for(const auto& line:{"DMSG AAAA BBBB private","DSTA AAAA BBBB 100 private","DSCH AAAA BBBB ANprivate","DRES AAAA BBBB FNprivate SI0 TR","DZZZ AAAA BBBB XXprivate","BCTM AAAA ADC/1.0 1515 token","FRCM CCCC +TCP4 ADC/1.0 token"}){
        const auto message=parse(line);require(std::get<RoutingError>(router.prepare(message.source=="CCCC"?id(3):id(1),message))==RoutingError::invalid_message,"Non-connection/scope escaped whitelist");
    }
    require(std::get<RoutingError>(router.prepare(id(9),parse("DCTM AAAA BBBB ADC/1.0 1 token")))==RoutingError::invalid_state,"Missing sender accepted");
    require(std::get<RoutingError>(router.prepare(id(1),parse("DRCM CCCC DDDD ADC/1.0 token")))==RoutingError::wrong_source,"Wrong source lost precedence");
    require(std::get<CtmRoutingError>(legacy_ctm.prepare(id(1),parse("DRCM AAAA DDDD ADC/1.0 token")))==CtmRoutingError::invalid_message,"Legacy CTM accepts RCM");
    require(std::get<RcmRoutingError>(legacy_rcm.prepare(id(3),parse("DCTM CCCC BBBB ADC/1.0 1 token")))==RcmRoutingError::invalid_message,"Legacy RCM accepts CTM");
    auto route=prepared(router,1,"AAAA","BBBB",false,true);auto copy=route;unsigned calls=0;
    const ConnectionRouter::Sink sink=[&](auto,const auto&){++calls;return true;};
    require(std::get<RoutingError>(foreign.dispatch(route,sink))==RoutingError::foreign_plan&&!foreign.cancel(route),"Foreign common router accepted plan");
    require(std::get<RoutingError>(legacy_ctm.dispatch(route,sink))==RoutingError::foreign_plan,"Typed facade accepted foreign common plan");
    require(std::get<RoutingError>(router.dispatch(route,{}))==RoutingError::missing_sink&&route.plan().state()==transfers::State::prepared,"Missing sink consumed plan");
    require(std::get<ConnectionDispatchReport>(router.dispatch(route,sink))==ConnectionDispatchReport{2,0,0}&&calls==2,"Common dispatch failed");
    require(std::get<RoutingError>(router.dispatch(copy,sink))==RoutingError::already_dispatched&&calls==2,"Copy replayed across command adapters");
    auto canceled=prepared(router,3,"CCCC","DDDD",true);require(router.cancel(canceled)&&!router.cancel(canceled)&&std::get<RoutingError>(router.dispatch(canceled,sink))==RoutingError::already_dispatched,"Canceled route dispatched");
    alignas(ConnectionRouter) unsigned char storage[sizeof(ConnectionRouter)];auto* first=new(storage)ConnectionRouter(core);auto old=prepared(*first,1,"AAAA","BBBB");first->~ConnectionRouter();auto* replacement=new(storage)ConnectionRouter(core);
    const bool rejected=std::get<RoutingError>(replacement->dispatch(old,sink))==RoutingError::foreign_plan;replacement->~ConnectionRouter();require(rejected,"Router address reuse admitted old route");
}
void lifecycle_and_callback_replacement() {
    users::SessionManager sessions(12);add(sessions,1,"AAAA");add(sessions,2,"BBBB",profile({}));add(sessions,3,"CCCC",profile({}));add(sessions,4,"DDDD");transfers::Core core(sessions);ConnectionRouter router(core);
    auto stale=prepared(router,1,"AAAA","BBBB",false,true);remove(sessions,2);add(sessions,5,"BBBB",profile({}));unsigned calls=0;
    require(std::get<RoutingError>(router.dispatch(stale,[&](auto,const auto&){++calls;return true;}))==RoutingError::recipient_unavailable&&calls==0,"Reused SID or missing target echoed stale request");
    auto current=prepared(router,1,"AAAA","BBBB",false,true);require(current.recipients()[0]==id(5),"New route missed replacement lifetime");
    auto original=current;auto replacement=prepared(router,3,"CCCC","DDDD",true,true);const auto old_frame=current.frame();
    std::array<std::uint64_t,2> delivered{};unsigned count=0;
    const auto result=router.dispatch(current,[&](auto recipient,const auto& frame){require(frame==old_frame&&frame->message().kind==Command::ctm,"Callback replaced frame/command during delivery");delivered[count++]=recipient.value();if(count==1)current=replacement;return true;});
    require(std::get<ConnectionDispatchReport>(result)==ConnectionDispatchReport{2,0,0}&&delivered==std::array<std::uint64_t,2>{5,1}&&original.plan().state()==transfers::State::queued&&current.plan().state()==transfers::State::prepared,"Callback retargeted fixed audience or consumed replacement");
    auto closing=prepared(router,1,"AAAA","BBBB");require(!sessions.begin_close(id(5),users::CloseCause::shutdown,{}),"Begin close failed");
    require(std::get<RoutingError>(router.dispatch(closing,[](auto,const auto&){return true;}))==RoutingError::recipient_unavailable,"Closing target accepted");
    auto echo=prepared(router,3,"CCCC","DDDD",true,true);
    require(std::get<ConnectionDispatchReport>(router.dispatch(echo,[&](auto,const auto&){remove(sessions,3);add(sessions,6,"CCCC",profile({}));return true;}))==ConnectionDispatchReport{1,0,1},"Echo went to replacement source SID");
    auto missing_source=prepared(router,1,"AAAA","DDDD");remove(sessions,1);add(sessions,7,"AAAA");
    require(std::get<RoutingError>(router.dispatch(missing_source,[](auto,const auto&){return true;}))==RoutingError::invalid_state,"Replaced source lifetime accepted");
}
void faults_and_isolation() {
    users::SessionManager sessions(8);add(sessions,1,"AAAA");add(sessions,2,"BBBB",profile({}));add(sessions,3,"CCCC",profile({}));add(sessions,4,"DDDD");transfers::Core core(sessions);ConnectionRouter router(core);
    for(bool reverse:{false,true}){
        const auto message=parse(std::string(reverse?"ERCM CCCC DDDD ":"ECTM AAAA BBBB ")+std::string(32,'P')+" "+(reverse?"":"65535 ")+std::string(128,'T'));
        bool success=false;unsigned failures=0;
        for(long budget=0;budget<64;++budget){fail_after=budget;try{auto value=router.prepare(reverse?id(3):id(1),message);fail_after=-1;require(std::holds_alternative<RoutedConnection>(value),"Fault changed admission");success=true;break;}catch(const std::bad_alloc&){fail_after=-1;++failures;require(sessions.size()==4,"Fault mutated registry");}}
        require(success&&failures>=5,"Connection preparation faults not covered");
        auto value=router.prepare(reverse?id(3):id(1),message);auto route=std::get<RoutedConnection>(std::move(value));auto copy=route;
        const ConnectionRouter::Sink sink=[](auto,const auto&){return true;};
        fail_after=0;const auto report=router.dispatch(route,sink);const auto replay=router.dispatch(copy,sink);fail_after=-1;
        require(std::get<ConnectionDispatchReport>(report)==ConnectionDispatchReport{2,0,0}&&std::get<RoutingError>(replay)==RoutingError::already_dispatched,"Dispatch allocated or replayed");
    }
    auto refused=prepared(router,1,"AAAA","BBBB",false,true);
    require(std::get<ConnectionDispatchReport>(router.dispatch(refused,[](auto recipient,const auto&){return recipient==id(1);}))==ConnectionDispatchReport{1,1,0},"Target refusal suppressed independent echo");
    struct Failure{};auto partial=prepared(router,3,"CCCC","DDDD",true,true);unsigned calls=0;
    try{(void)router.dispatch(partial,[&](auto,const auto&)->bool{if(++calls==2)throw Failure{};return true;});require(false,"Sink throw swallowed");}catch(const Failure&){}
    require(std::get<RoutingError>(router.dispatch(partial,[](auto,const auto&){return true;}))==RoutingError::already_dispatched,"Partial exception permitted duplicate route");
}
}
int main(){try{sid_matrix();admission_and_domains();lifecycle_and_callback_replacement();faults_and_isolation();std::cout<<"PASS: common connection SID routing, fixed audience, domains, lifetimes and faults\n";}catch(const std::exception& e){fail_after=-1;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
