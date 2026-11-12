#include "dc24h/adc/result_routing.hpp"
#include "dc24h/adc/serializer.hpp"
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

const std::string root(39,'A');
std::string wire(std::string from="AAAC",std::string to="AAAB",std::string token="opaque") {
    return "DRES "+from+" "+to+" XXunknown XXagain FN/share/Żółw\\s😀.txt SI00042 SL-0 TR"+root+
        (token.empty()?"":" TO"+token);
}
search::ActivePlan active(search::Core& core,std::uint64_t owner=1,std::string token="opaque",users::TimePoint now={}) {
    search::QueryData query; query.include={"Żółw"}; query.token=std::move(token);
    return std::get<search::ActivePlan>(core.start(id(owner),sid(owner),query,now));
}
RoutedResult prepared(ResultRouter& router,std::string body=wire(),users::TimePoint now={}) {
    return std::get<RoutedResult>(router.prepare(id(2),parse(body),now));
}
void direct_wire_and_validation() {
    users::SessionManager sessions(16); add(sessions,1); add(sessions,2); add(sessions,3); add(sessions,4,2);
    search::Core core(sessions); ResultRouter router(core); const auto query=active(core);
    const auto expected_wire=wire()+"\n";
    auto message=parse(wire()); const auto result=std::get<RoutedResult>(router.prepare(id(2),message,{}));
    message.parameters.clear(); auto copy=result; std::size_t calls=0;
    require(result.sender()==id(2) && result.recipient()==id(1) && result.search()==query.handle().search(),"Result did not bind owner/generation");
    const ResultRouter::Sink sink=[&](auto target,const auto& frame){
        ++calls; require(target==id(1) && frame.get()==result.frame().get() && frame->wire()==expected_wire,"DRES fields/target/frame changed"); return true;
    };
    allocation_budget=0; const auto report=router.dispatch(result,sink,{}); allocation_budget=-1;
    require(std::get<search::DispatchReport>(report)==search::DispatchReport{1,0,0,0} && calls==1,"Dispatch allocated or fanned out");
    require(std::get<ResultRoutingError>(router.dispatch(copy,sink,{}))==ResultRoutingError::already_dispatched,"Copied result replayed");
    require(core.find_search(id(1),"opaque",{})==query.handle().search(),"Result completed/changed search window");
    // Independent responses may share one active search, without deduplication.
    require(std::get<search::DispatchReport>(router.dispatch(prepared(router),[](auto,const auto&){return true;},{})).queued==1,"Second response refused");
    auto tokenless=prepared(router,wire("AAAC","AAAB",""));
    require(!tokenless.search() && std::get<search::DispatchReport>(router.dispatch(tokenless,[](auto,const auto&){return true;},{})).queued==1,"Tokenless compatibility lost");
    auto self=std::get<RoutedResult>(router.prepare(id(1),parse(wire("AAAB","AAAB","")),{}));
    calls=0; require(std::get<search::DispatchReport>(router.dispatch(self,[&](auto target,const auto&){++calls;return target==id(1);},{})).queued==1 && calls==1,"Self result duplicated");
    for (const auto& [body,error]:std::vector<std::pair<std::string,ResultRoutingError>>{
        {wire("AAAB"),ResultRoutingError::wrong_source},
        {wire("AAAC","AAAZ"),ResultRoutingError::recipient_unavailable},
        {wire("AAAC","AAAE"),ResultRoutingError::recipient_unavailable},
        {wire("AAAC","AAAD"),ResultRoutingError::inactive_search},
        {wire("AAAC","AAAB","Opaque"),ResultRoutingError::inactive_search},
        {"DRES AAAC AAAB FNfile SI0",ResultRoutingError::invalid_message},
        {"ERES AAAC AAAB FNfile SI0 TR"+root,ResultRoutingError::invalid_message},
        {"BRES AAAC FNfile SI0 TR"+root,ResultRoutingError::invalid_message}}) {
        require(std::get<ResultRoutingError>(router.prepare(id(2),parse(body),{}))==error,"Result validation priority differs");
    }
    require(std::get<ResultRoutingError>(router.prepare(id(4),parse(wire()),{}))==ResultRoutingError::invalid_state,"Unready responder accepted");
    auto huge=parse(wire()); huge.source=std::string(1024*1024,'x');
    allocation_budget=0; const auto bad_source=router.prepare(id(2),huge,{}); allocation_budget=-1;
    require(std::get<ResultRoutingError>(bad_source)==ResultRoutingError::wrong_source,"Huge source allocated");
    huge=parse(wire()); huge.parameters={std::string(1024*1024,'x')};
    allocation_budget=0; const auto oversized=router.prepare(id(2),huge,{}); allocation_budget=-1;
    require(std::get<ResultRoutingError>(oversized)==ResultRoutingError::invalid_message,"Huge result allocated before bounds");
}
void attempt_and_generations() {
    users::SessionManager sessions(8); add(sessions,1); add(sessions,2);
    search::Core core(sessions,{8,8,1s}); ResultRouter router(core), foreign(core);
    const auto query=active(core); auto result=prepared(router); auto copy=result;
    require(std::get<ResultRoutingError>(foreign.dispatch(result,[](auto,const auto&){return true;},{}))==ResultRoutingError::foreign_plan,"Foreign router accepted plan");
    require(std::get<ResultRoutingError>(router.dispatch(result,{},{}))==ResultRoutingError::missing_sink,"Empty sink accepted");
    require(core.complete(query),"Search cancellation failed");
    const auto replacement=active(core);
    require(replacement.handle().search().id!=query.handle().search().id,"Search generation reused");
    require(std::get<ResultRoutingError>(router.dispatch(result,[](auto,const auto&){return true;},{}))==ResultRoutingError::inactive_search,"Prepared old token generation dispatched");
    require(std::get<ResultRoutingError>(router.dispatch(copy,[](auto,const auto&){return true;},{}))==ResultRoutingError::already_dispatched,"Failed attempt replayed");
    auto refused=prepared(router);
    require(std::get<search::DispatchReport>(router.dispatch(refused,[](auto,const auto&){return false;},{}))==search::DispatchReport{0,0,1,0},"Queue refusal became success");
    auto throwing=prepared(router); bool threw=false;
    try { (void)router.dispatch(throwing,[](auto,const auto&)->bool{throw std::runtime_error("sink");},{}); }
    catch (const std::runtime_error&) { threw=true; }
    require(threw && std::get<ResultRoutingError>(router.dispatch(throwing,[](auto,const auto&){return true;},{}))==ResultRoutingError::already_dispatched,"Throwing sink retried");
    auto expires=prepared(router);
    require(std::get<ResultRoutingError>(router.dispatch(expires,[](auto,const auto&){return true;},users::TimePoint{}+1s))==ResultRoutingError::inactive_search,"Exact expiry boundary ignored");
    require(!core.find_search(id(1),"opaque",users::TimePoint{}+1s),"Result renewed the deadline");
}
void lifetime_rechecks() {
    for (const bool close_sender:{false,true}) {
        users::SessionManager sessions(8); add(sessions,1); add(sessions,2);
        search::Core core(sessions); ResultRouter router(core); (void)active(core);
        const auto result=prepared(router); const auto removed=close_sender?id(2):id(1);
        require(!sessions.finish_close(removed,users::CloseCause::peer_closed,{}) && !sessions.remove(removed),"Cleanup failed");
        add(sessions,3,3,{},close_sender?"AAAC":"AAAB");
        std::size_t calls=0;
        require(std::get<ResultRoutingError>(router.dispatch(result,[&](auto,const auto&){++calls;return true;},{}))==
            (close_sender?ResultRoutingError::invalid_state:ResultRoutingError::recipient_unavailable) && !calls,"Reused SID replaced prepared lifetime");
    }
    users::SessionManager sessions(8); add(sessions,1); add(sessions,2);
    search::Core core(sessions); ResultRouter router(core); auto result=prepared(router,wire("AAAC","AAAB",""));
    require(!sessions.begin_close(id(1),users::CloseCause::peer_closed,{}),"Begin close failed");
    require(std::get<ResultRoutingError>(router.dispatch(result,[](auto,const auto&){return true;},{}))==ResultRoutingError::recipient_unavailable,"Closing recipient accepted");
}
void preparation_faults() {
    users::SessionManager sessions(8); add(sessions,1); add(sessions,2);
    search::Core core(sessions); ResultRouter router(core); const auto query=active(core);
    const auto body=wire()+" XX"+std::string(256,'x'); const auto message=parse(body);
    const auto before=sessions.statistics(); bool failed=false,success=false;
    for (long budget=0;budget<200 && !success;++budget) {
        allocation_budget=budget;
        try { auto result=router.prepare(id(2),message,{}); allocation_budget=-1;
            require(std::holds_alternative<RoutedResult>(result),"Fault changed result semantics"); success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1; failed=true; }
        require(core.find_search(id(1),"opaque",{})==query.handle().search() && sessions.statistics()==before &&
            std::get<std::string>(serialize(message))==body+"\n","Fault mutated search/session/input");
    }
    require(failed && success,"Preparation fault campaign missed a path");
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

void queues_and_failure_isolation() {
    LoginLimits limits; limits.output_per_client=1024; limits.output_total=4096;
    LoginHub hub(limits); const auto target=join(hub,1), sender=join(hub,2); (void)join(hub,3); drain(hub,1);drain(hub,2);
    const auto body="DRES "+sender+" "+target+" FN/share/"+std::string(300,'x')+" SI0 TR"+root;
    hub.receive(2,parse(body)); const auto pinned=hub.output(1); const auto* pointer=pinned.data();
    const auto charge=hub.queued_bytes(); hub.consume(1,5);
    require(hub.queued_bytes()==charge && hub.output(1).data()==pointer+5,"Partial result send changed charge/pinned offset");
    hub.receive(2,parse(body));
    require(hub.output(1).data()==pointer+5 && hub.output(2).empty() && hub.output(3).empty(),"DRES echoed or disturbed pinned front");
    hub.receive(2,parse(body));
    require(hub.must_close(1) && hub.ready(2) && hub.ready(3) && hub.output(1).data()==pointer+5,"Slow result recipient affected healthy peers");
    require(hub.queued_bytes()==2*(body.size()+1),"Refused result was charged");
    hub.disconnect(1,false); require(hub.queued_bytes()==0 && hub.chat_statistics()==chat::Statistics{},"Result cleanup leaked charge/chat counters");
    // Walk allocation failures through frame preparation and target queue growth.
    bool source_failure=false,target_failure=false,success=false;
    for (int prefill=0;prefill<8 && !(source_failure && target_failure && success);++prefill) {
      for (long budget=0;budget<120 && !(source_failure && target_failure && success);++budget) {
        LoginHub isolated; const auto a=join(isolated,1), b=join(isolated,2); drain(isolated,1);
        const auto message=parse("DRES "+b+" "+a+" FN/share/"+std::string(128,'x')+" SI0 TR"+root);
        // deque block expansion is exercised over enough queued result frames.
        for (int n=0;n<prefill;++n) { isolated.receive(2,message); }
        allocation_budget=budget; isolated.receive(2,message); allocation_budget=-1;
        source_failure=source_failure || isolated.must_close(2);
        target_failure=target_failure || (isolated.must_close(1) && isolated.ready(2));
        success=success || (isolated.ready(1) && isolated.ready(2));
        require(!(isolated.must_close(1) && isolated.must_close(2)),"Target queue allocation failure also closed source");
    }
    }
    require(source_failure && target_failure && success,"Receive allocation campaign missed source/target failure or success");
}
} // namespace
int main() {
    try { direct_wire_and_validation(); attempt_and_generations(); lifetime_rechecks(); preparation_faults(); queues_and_failure_isolation();
        std::cout<<"PASS: result target/token/lifetime routing, queues and allocation isolation\n";
    } catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
