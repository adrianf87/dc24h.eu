#include "dc24h/adc/search_routing.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/sid.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long allocation_budget = -1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (allocation_budget == 0) { throw std::bad_alloc(); }
    if (allocation_budget > 0) { --allocation_budget; }
    if (auto* value = std::malloc(size == 0 ? 1 : size)) { return value; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete(void* value, std::size_t) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {
using namespace dc24h;
using namespace dc24h::adc;
using namespace std::chrono_literals;
void require(bool ok,const char* why) { if (!ok) { throw std::runtime_error(why); } }
users::SessionId id(std::uint64_t n) { return *users::SessionId::from_value(n); }
std::string sid(std::uint64_t n) { return Sid::from_value(static_cast<std::uint32_t>(n))->str(); }
Message parse(std::string_view body) { return std::get<Message>(parse_line(body)); }
void add(users::SessionManager& sessions,std::uint64_t n,int phase=3,std::vector<std::string> features={},std::string source={}) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n),users::Transport::adc,{})),"Add failed");
    if (phase==0) { return; }
    require(!sessions.start_negotiation(id(n),{}),"Negotiation failed");
    if (phase==1) { return; }
    users::ProfileData profile; profile.features=std::move(features);
    require(!sessions.identify(id(n),*users::User::create(*users::Identity::create(source.empty()?sid(n):source,
        "CID"+std::to_string(n),"Nick"+std::to_string(n)),*users::Profile::create(profile)),{}),"Identify failed");
    if (phase==3) { require(!sessions.mark_ready(id(n),{}),"Ready failed"); }
}
RoutedSearch prepared(SearchRouter& router,std::string_view wire,users::TimePoint now={}) {
    return std::get<RoutedSearch>(router.prepare(id(1),parse(wire),now));
}
void scopes_and_wire() {
    users::SessionManager sessions(16); add(sessions,1,3,{"TCP4"}); add(sessions,2,3,{"TCP4"}); add(sessions,3,3,{"UDP4"}); add(sessions,4,2);
    search::Core core(sessions); SearchRouter router(core);
    const std::vector<std::pair<std::string,std::vector<std::uint64_t>>> cases{
        {"BSCH AAAB",{1,2,3}}, {"FSCH AAAB +TCP4-UDP4",{1,2}}, {"DSCH AAAB AAAD",{3}},
        {"ESCH AAAB AAAD",{3,1}}, {"ESCH AAAB AAAB",{1}}, {"FSCH AAAB +ZZZ0",{}}};
    for (const auto& [header,expected]:cases) {
        const auto body=header+" TR"+std::string(39,'A')+" ANŻółw\\s😀 EXISO GE-0 TY01 XXone XXtwo";
        const auto routed=prepared(router,body); auto copy=routed; std::vector<std::uint64_t> recipients;
        const auto* shared=routed.frame().get();
        const auto report=router.dispatch(routed,[&](users::SessionId recipient,const SharedSearchFrame& frame) {
            require(frame.get()==shared && frame->wire()==body+'\n',"Frame storage/fields/spelling changed");
            recipients.push_back(recipient.value()); return true;
        },{});
        require(recipients==expected && std::get<search::DispatchReport>(report).queued==expected.size(),"B/F/D/E recipient scope differs");
        require(std::get<SearchRoutingError>(router.dispatch(copy,[](auto,const auto&){return true;},{}))==SearchRoutingError::already_dispatched,
            "Copied publication replayed");
    }
    auto query=prepared(router,"BSCH AAAB ANoriginal");
    search::Core foreign_core(sessions); SearchRouter foreign(foreign_core);
    require(std::get<SearchRoutingError>(foreign.dispatch(query,[](auto,const auto&){return true;},{}))==SearchRoutingError::foreign_plan,"Foreign Core dispatched a plan");
    require(std::get<SearchRoutingError>(router.dispatch(query,{},{}))==SearchRoutingError::missing_sink,"Missing sink accepted");
    require(std::holds_alternative<search::DispatchReport>(router.dispatch(query,[](auto,const auto&){return true;},{})),"Foreign/missing-sink check consumed the attempt");
    for (const auto& [body,error]:std::vector<std::pair<std::string,SearchRoutingError>>{
        {"BSCH AAAC ANx",SearchRoutingError::wrong_source},{"BSCH AAAB",SearchRoutingError::no_criteria},
        {"BSCH AAAB ANx TO",SearchRoutingError::invalid_message},{"DSCH AAAB AAAZ ANx",SearchRoutingError::recipient_unavailable}}) {
        require(std::get<SearchRoutingError>(router.prepare(id(1),parse(body),{}))==error,"Routing validation/precedence differs");
    }
    auto huge=parse("BSCH AAAB ANx"); huge.source=std::string(1024*1024,'x');
    allocation_budget=0; const auto refused=router.prepare(id(1),huge,{}); allocation_budget=-1;
    require(std::get<SearchRoutingError>(refused)==SearchRoutingError::wrong_source,"Oversized source copied before validation");
    search::QueryData criterion; criterion.include={"x"};
    require(std::get<search::Error>(core.prepare(id(1),"AAAB",criterion, {}, {search::Scope::direct,{}}))==search::Error::invalid_audience,
        "Core accepted a directed audience without a target");
}
void lifetime_features_and_tokens() {
    users::SessionManager sessions(16); add(sessions,1,3,{"TCP4"}); add(sessions,2,3,{"TCP4"});
    search::Core core(sessions,{8,8,1s}); SearchRouter router(core);
    auto feature=prepared(router,"FSCH AAAB +TCP4 ANx");
    auto profile=sessions.find(id(2))->user()->profile().data(); profile.features={"UDP4"};
    require(!sessions.update_user(id(2),*users::User::create(*sessions.find(id(2))->identity(),*users::Profile::create(profile)),{}),"Feature update failed");
    add(sessions,3,3,{"TCP4"});
    require(std::get<search::DispatchReport>(router.dispatch(feature,[](auto,const auto&){return true;},{}))==search::DispatchReport{1,1,0,0},"Changed features or late join ignored snapshot lifetimes");
    auto directed=prepared(router,"DSCH AAAB AAAC ANx TOtarget");
    require(!sessions.finish_close(id(2),users::CloseCause::peer_closed,{}) && !sessions.remove(id(2)),"Target cleanup failed");
    add(sessions,4,3,{},"AAAC"); std::size_t calls=0;
    require(std::get<search::DispatchReport>(router.dispatch(directed,[&](auto,const auto&){++calls;return true;},{}))==search::DispatchReport{0,1,0,0} && calls==0,
        "Reused SID replaced the selected lifetime");
    require(!core.find_search(id(1),"target",{}),"No-delivery search retained its token");
    auto active=prepared(router,"BSCH AAAB ANx TOopaque");
    require(std::get<SearchRoutingError>(router.prepare(id(1),parse("BSCH AAAB ANx TOopaque"),{}))==SearchRoutingError::duplicate_token,"Duplicate active token forwarded");
    require(router.cancel(active),"Cancellation failed");
    auto reused=prepared(router,"BSCH AAAB ANx TOopaque");
    require(!router.cancel(active),"Old cancellation removed a replacement");
    require(std::get<SearchRoutingError>(router.dispatch(reused,[](auto,const auto&){return true;},users::TimePoint{}+1s))==SearchRoutingError::inactive_search,"Expired query dispatched");
    auto empty=prepared(router,"FSCH AAAB +ZZZ0 ANx TOempty",users::TimePoint{}+1s);
    require(std::get<search::DispatchReport>(router.dispatch(empty,[](auto,const auto&){return true;},users::TimePoint{}+1s)).queued==0 &&
        !core.find_search(id(1),"empty",users::TimePoint{}+1s),"Empty selection retained a token");
    auto throwing=prepared(router,"BSCH AAAB ANx TOthrow",users::TimePoint{}+1s); bool threw=false;
    try { (void)router.dispatch(throwing,[](auto,const auto&)->bool{throw std::runtime_error("sink");},users::TimePoint{}+1s); }
    catch (const std::runtime_error&) { threw=true; }
    require(threw && std::get<SearchRoutingError>(router.dispatch(throwing,[](auto,const auto&){return true;},users::TimePoint{}+1s))==SearchRoutingError::already_dispatched &&
        router.cancel(throwing),"Thrown sink retried or lost explicit cleanup");
}
void multipage_and_faults() {
    users::SessionManager sessions(300); for (std::uint64_t n=1;n<=270;++n) { add(sessions,n); }
    add(sessions,271,2); search::Core core(sessions); SearchRouter router(core);
    const auto routed=prepared(router,"BSCH AAAB ANx TOtoken"); std::size_t count=0;
    const SearchRouter::Sink sink=[&](auto,const auto& frame){require(frame.get()==routed.frame().get(),"Fanout copied a frame");++count;return true;};
    allocation_budget=0; const auto report=router.dispatch(routed,sink,{}); allocation_budget=-1;
    require(std::get<search::DispatchReport>(report)==search::DispatchReport{270,0,0,0} && count==270,"Multipage routing allocated or missed a ready client");
    require(router.cancel(routed),"Token cleanup failed");
    const auto message=parse("BSCH AAAB AN"+std::string(128,'x')+" TO"+std::string(64,'t'));
    const auto before=sessions.statistics(); bool failed=false,success=false;
    for (long budget=0;budget<200 && !success;++budget) {
        allocation_budget=budget;
        try { auto result=router.prepare(id(1),message,{}); allocation_budget=-1;
            require(std::holds_alternative<RoutedSearch>(result),"Preparation fault became semantic refusal");
            require(router.cancel(std::get<RoutedSearch>(result)),"Prepared token missing"); success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1;failed=true; }
        require(!core.find_search(id(1),std::string(64,'t'),{}) && sessions.statistics()==before,"Failed preparation published a token/model mutation");
    }
    require(failed && success,"Routing allocation campaign missed failure/success");
}
void drain(LoginHub& hub,std::uint64_t owner) {
    while (!hub.output(owner).empty() && !hub.must_close(owner)) { hub.consume(owner,hub.output(owner).size()); }
}
std::string join(LoginHub& hub,std::uint64_t owner) {
    hub.connect(owner,"127.0.0.1"); hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size()); const std::string source(hub.output(owner).substr(5,4)); hub.consume(owner,hub.output(owner).size());
    TigerDigest pid{}; pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+source+" NIUser"+std::to_string(owner)+" ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)));
    drain(hub,owner); require(hub.ready(owner),"Guest not ready"); return source;
}
void queues_and_slow_recipient() {
    LoginLimits limits; limits.output_per_client=1024; limits.output_total=4096;
    LoginHub hub(limits); const auto source=join(hub,1), second=join(hub,2); (void)join(hub,3); drain(hub,1); drain(hub,2);
    for (int n=0;n<3;++n) { hub.receive(1,parse("DSCH "+source+" "+second+" AN"+std::string(256,'x'))); }
    const auto pinned=hub.output(2); const auto* pointer=pinned.data();
    const auto body="BSCH "+source+" AN"+std::string(256,'y'); hub.receive(1,parse(body));
    require(hub.must_close(2) && hub.ready(1) && hub.ready(3),"Slow recipient closed healthy search peers");
    require(hub.output(2).data()==pointer && hub.output(2)==pinned,"Queue refusal invalidated a pinned front");
    require(hub.output(1)==body+'\n' && hub.output(1).data()==hub.output(3).data(),"Search queues do not share one immutable frame");
    const auto charge=hub.queued_bytes(); hub.consume(1,5);
    require(hub.queued_bytes()==charge && hub.output(1).data()==hub.output(3).data()+5,"Partial send altered logical charge or another offset");
    hub.disconnect(2,false); require(hub.queued_bytes()==2*(body.size()+1),"Slow queue cleanup leaked charge");
    drain(hub,3); hub.disconnect(1,false); hub.disconnect(3,false);
    require(hub.queued_bytes()==0 && hub.chat_statistics()==chat::Statistics{},"Search leaked queue charges or chat counters");
}
} // namespace
int main() {
    try { scopes_and_wire(); lifetime_features_and_tokens(); multipage_and_faults(); queues_and_slow_recipient();
        std::cout<<"PASS: live search routing scopes, shared queues, lifetimes, tokens and allocation rollback\n";
    } catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
