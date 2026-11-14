#include "dc24h/adc/connection_errors.hpp"
#include "dc24h/adc/status_routing.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/serializer.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long fail_after=-1; unsigned faults=0; }
[[gnu::noinline]] void* operator new(std::size_t n) {
    if(fail_after==0){fail_after=-1;++faults;throw std::bad_alloc();}
    if(fail_after>0){--fail_after;}
    if(auto* p=std::malloc(n?n:1)){return p;}throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void* p) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void* p) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
namespace {
using namespace dc24h;
using namespace dc24h::adc;
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
users::SessionId id(unsigned n){return *users::SessionId::from_value(n);}
Message parse(std::string_view s){return std::get<Message>(parse_line(s));}
users::Profile profile(bool active){users::ProfileData p;if(active){p.features={"TCP4"};}return *users::Profile::create(p);}
void add(users::SessionManager& s,unsigned n,std::string key,bool active=false){
    require(std::holds_alternative<const users::Session*>(s.add(id(n),users::Transport::adc,{})),"Add failed");
    require(!s.start_negotiation(id(n),{}),"Start failed");
    require(!s.identify(id(n),*users::User::create(*users::Identity::create(key,"CID"+std::to_string(n),"User"+std::to_string(n)),profile(active)),{}),"Identify failed");
    require(!s.mark_ready(id(n),{}),"Ready failed");
}
void update(users::SessionManager& s,unsigned n,bool active){require(!s.update_user(id(n),*users::User::create(*s.find(id(n))->identity(),profile(active)),{}),"Update failed");}
void remove(users::SessionManager& s,unsigned n){require(!s.finish_close(id(n),users::CloseCause::peer_closed,{})&&!s.remove(id(n)),"Remove failed");}
void error_policy(){
    for(auto command:{Command::ctm,Command::rcm})for(auto error:{ConnectionRoutingError::invalid_state,ConnectionRoutingError::wrong_source,
        ConnectionRoutingError::invalid_message,ConnectionRoutingError::recipient_unavailable,ConnectionRoutingError::inactive_sender,
        ConnectionRoutingError::foreign_plan,ConnectionRoutingError::missing_sink,ConnectionRoutingError::already_dispatched,
        ConnectionRoutingError::incompatible_connectivity,ConnectionRoutingError::disabled,ConnectionRoutingError::forbidden}){
        fail_after=0;const auto f=connection_failure(error,command);const auto remaining=fail_after;fail_after=-1;
        require(remaining==0,"Failure policy allocated");
        if(f.action==FailureAction::abort){require(f.code.empty()&&f.description.empty(),"Internal error exposed client diagnostic");}
        else{const auto code=StatusCode::parse(f.code);require(code&&code->severity()==(f.action==FailureAction::terminate?Severity::fatal:Severity::recoverable),"Failure action/severity disagree");}
    }
    require(connection_failure(static_cast<ConnectionRoutingError>(99),Command::ctm).action==FailureAction::abort&&
        connection_failure(ConnectionRoutingError::invalid_message,Command::unknown).action==FailureAction::abort,"Unknown error escaped fail-closed policy");
}
void audiences_and_ownership(){
    users::SessionManager s(8);add(s,1,"AAAA",true);add(s,2,"BBBB");add(s,3,"CCCC",true);StatusRouter router(s),foreign(s);
    const std::array<std::string,5> headers{"DSTA AAAA BBBB","ESTA AAAA BBBB","BSTA AAAA","FSTA AAAA +TCP4","FSTA AAAA -TCP4"};
    for(unsigned scope=0;scope<headers.size();++scope)for(const auto code:{"000","100","141","142","241","242","299"}){
        const auto number=StatusCode::parse(code)->error();
        const auto token=number==41||number==42?"Żółw\\sTransferPrivate\\\\x":"TransferPrivate";
        auto input=parse(headers[scope]+" "+code+" PeerPrivate PRADCS/1.0 TO"+token+" XXdiscard PDdiscard");
        const auto expected=std::get<std::string>(serialize(public_status(input)));
        auto route=std::get<StatusDelivery>(router.prepare(id(1),input));auto copy=route;
        const std::vector<users::SessionId> ids=scope==0?std::vector{id(2)}:scope==1?std::vector{id(2),id(1)}:scope==2?std::vector{id(1),id(2),id(3)}:scope==3?std::vector{id(1),id(3)}:std::vector{id(2)};
        require(std::vector(route.recipients().begin(),route.recipients().end())==ids,"Wrong status audience");
        input.parameters.assign({"100","mutated"});unsigned calls=0;
        const StatusRouter::Sink sink=[&](auto target,const auto& frame){require(target==ids[calls++]&&frame->wire()==expected&&frame->code()==*StatusCode::parse(code),"Status changed during dispatch");return true;};
        require(std::get<StatusRoutingError>(foreign.dispatch(route,sink))==StatusRoutingError::foreign_plan,"Foreign status domain accepted");
        require(std::get<StatusRoutingError>(router.dispatch(route,{}))==StatusRoutingError::missing_sink,"Missing sink consumed request");
        fail_after=0;const auto report=router.dispatch(route,sink);const auto remaining=fail_after;fail_after=-1;
        require(remaining==0&&std::get<StatusDispatchReport>(report).queued==ids.size()&&calls==ids.size(),"Status dispatch allocated or lost recipients");
        require(std::get<StatusRoutingError>(router.dispatch(copy,sink))==StatusRoutingError::already_dispatched,"Copied status replayed");
        require(s.find(id(1))->ready()&&s.find(id(2))->ready(),"Forwarded fatal STA disconnected hub user");
    }
    auto self=std::get<StatusDelivery>(router.prepare(id(1),parse("ESTA AAAA AAAA 100 Self")));require(self.recipients().size()==1,"Self echo duplicated");
    require(router.cancel(self)&&!router.cancel(self),"Status cancel not shared/idempotent");
    require(std::get<StatusRoutingError>(router.prepare(id(1),parse("DSTA AAAA ZZZZ 142 Missing")))==StatusRoutingError::recipient_unavailable,"Missing SID accepted");
    require(std::get<StatusRoutingError>(router.prepare(id(1),parse("DSTA BBBB CCCC 142 Forged")))==StatusRoutingError::wrong_source,"Forged status source accepted");
}
void stale_and_reentrant(){
    users::SessionManager s(8);add(s,1,"AAAA",true);add(s,2,"BBBB");add(s,3,"CCCC",true);StatusRouter router(s);
    auto stale=std::get<StatusDelivery>(router.prepare(id(1),parse("DSTA AAAA BBBB 242 Old TOTransferPrivate")));
    remove(s,2);add(s,4,"BBBB");unsigned calls=0;
    const StatusRouter::Sink sink=[&](auto,const auto&){++calls;return true;};
    require(std::get<StatusDispatchReport>(router.dispatch(stale,sink)).canceled==1&&!calls,"STA retargeted reused SID");
    auto filtered=std::get<StatusDelivery>(router.prepare(id(1),parse("FSTA AAAA +TCP4 100 Filter")));
    update(s,3,false);update(s,4,true);
    require(std::get<StatusDispatchReport>(router.dispatch(filtered,sink))==StatusDispatchReport{1,0,1}&&calls==1,"FSTA expanded audience after INF change");
    auto original=std::get<StatusDelivery>(router.prepare(id(1),parse("BSTA AAAA 299 Original")));
    auto replacement=std::get<StatusDelivery>(router.prepare(id(1),parse("DSTA AAAA BBBB 100 Replacement")));
    const auto wire=original.frame()->wire();calls=0;
    const auto report=router.dispatch(original,[&](auto,const auto& frame){++calls;require(frame->wire()==wire,"Callback replaced live status frame");original=replacement;return true;});
    require(std::get<StatusDispatchReport>(report).queued==3&&calls==3,"Callback changed captured audience");
    auto throwing=std::get<StatusDelivery>(router.prepare(id(1),parse("ESTA AAAA BBBB 142 Exception")));
    bool threw=false;try{(void)router.dispatch(throwing,[](auto,const auto&)->bool{throw 7;});}catch(int){threw=true;}
    require(threw&&std::get<StatusRoutingError>(router.dispatch(throwing,sink))==StatusRoutingError::already_dispatched,"Throwing status sink replayed");
    auto echo=std::get<StatusDelivery>(router.prepare(id(1),parse("ESTA AAAA BBBB 142 Departing")));calls=0;
    const auto closed=router.dispatch(echo,[&](auto,const auto&){++calls;require(!s.force_close(id(1),users::CloseCause::peer_closed,{}),"Close failed");return true;});
    require(std::get<StatusDispatchReport>(closed)==StatusDispatchReport{1,0,1}&&calls==1,"Closed source received echo");
}
std::string drain(LoginHub& hub,unsigned who){std::string out;while(!hub.output(who).empty()&&!hub.must_close(who)){const std::string p(hub.output(who));out+=p;hub.consume(who,p.size());}return out;}
std::string join(LoginHub& hub,unsigned who){
    hub.connect(who,"127.0.0.1");hub.receive(who,parse("HSUP ADBASE ADTIGR"));hub.consume(who,hub.output(who).size());
    const std::string sid(hub.output(who).substr(5,4));hub.consume(who,hub.output(who).size());TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(who);
    hub.receive(who,parse("BINF "+sid+" NIErrorUser"+std::to_string(who)+" ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)));
    (void)drain(hub,who);return sid;
}
void live_allocation_isolation(){
    bool source_fault=false,target_fault=false,success=false;
    for(unsigned fill=0;fill<4;++fill)for(long budget=0;budget<100;++budget){
        LoginHub hub;const auto a=join(hub,1),b=join(hub,2);(void)join(hub,3);(void)drain(hub,1);(void)drain(hub,2);
        for(unsigned n=0;n<fill;++n){hub.receive(1,parse("DMSG "+a+" "+b+" Prefill"));}
        const auto message=parse("ESTA "+a+" "+b+" 242 "+std::string(128,'P')+" PRADCS/1.0 TOTransferPrivate");
        const auto before=faults;fail_after=budget;hub.receive(1,message);fail_after=-1;
        require(hub.ready(3),"STA allocation fault affected bystander");
        source_fault|=!hub.ready(1);target_fault|=!hub.ready(2);success|=before==faults;
        if(!hub.ready(2)){require(hub.ready(1),"Target insertion fault closed source");}
        for(unsigned n=1;n<=3;++n){hub.disconnect(n,false);}
        require(hub.queued_bytes()==0&&hub.users()==0,"STA fault leaked output/identity");
    }
    require(source_fault&&target_fault&&success,"Status fault sweep missed preparation/queue/success phases");
    // Validation during completed output is a recipient-local fault as well.
    LoginHub hub;const auto a=join(hub,1),b=join(hub,2);(void)join(hub,3);(void)drain(hub,1);(void)drain(hub,2);
    hub.receive(1,parse("DSTA "+a+" "+b+" 142 "+std::string(128,'P')+" TOTransferPrivate"));
    const auto size=hub.output(2).size();fail_after=0;(void)hub.consume(2,size);fail_after=-1;
    require(hub.must_close(2)&&hub.ready(1)&&hub.ready(3),"Output validation fault escaped recipient isolation");
    hub.receive(1,parse("DSTA "+a+" "+a+" 100 "+std::string(128,'P')));hub.consume(1,1);
    const auto pinned=hub.output(1);const auto* address=pinned.data();
    fail_after=0;hub.close(1,CloseReason::idle_timeout);fail_after=-1;
    require(hub.must_close(1)&&hub.ready(3)&&hub.output(1).data()==address&&hub.output(1)==pinned,"Terminal OOM changed pinned frame or stopped another peer");
    for(unsigned n=1;n<=3;++n){hub.disconnect(n,false);}require(hub.queued_bytes()==0,"Terminal fault leaked queue charges");
    LoginHub invalid;const auto offender=join(invalid,1);(void)join(invalid,2);(void)drain(invalid,1);(void)offender;
    const auto bad=parse("HSTA 999 PrivateMalformed");fail_after=0;invalid.receive(1,bad);fail_after=-1;
    require(invalid.must_close(1)&&invalid.ready(2),"Malformed STA diagnostic allocation escaped source isolation");
    for(bool reverse:{false,true}){
        LoginHub rejected;const auto source=join(rejected,1),other=join(rejected,2);(void)drain(rejected,1);
        const auto forged=parse(std::string(reverse?"DRCM ":"DCTM ")+other+" "+source+" ADC/1.0 "+(reverse?"":"1515 ")+"TransferPrivate");
        fail_after=0;rejected.receive(1,forged);fail_after=-1;
        require(rejected.must_close(1)&&rejected.ready(2),"CTM/RCM fatal diagnostic allocation escaped source isolation");
    }
}
void queue_refusal_isolation(){
    LoginLimits limits;limits.output_per_client=2048;limits.output_total=8192;
    LoginHub hub(limits);const auto a=join(hub,1),b=join(hub,2);(void)join(hub,3);(void)drain(hub,1);(void)drain(hub,2);
    hub.receive(1,parse("DMSG "+a+" "+b+" "+std::string(900,'P')));hub.consume(2,1);
    const auto pinned=hub.output(2);const auto* pointer=pinned.data();
    const auto status="ESTA "+a+" "+b+" 242 "+std::string(1500,'S')+" TOTransferPrivate";
    hub.receive(1,parse(status));
    require(hub.must_close(2)&&hub.ready(1)&&hub.ready(3)&&drain(hub,1)==status+'\n',"Status queue cap affected unrelated sender/echo");
    require(hub.output(2).data()==pointer&&hub.output(2)==pinned,"Status refusal moved pinned recipient frame");
    for(unsigned n=1;n<=3;++n){hub.disconnect(n,false);}require(hub.queued_bytes()==0,"Queue refusal leaked charges");
}
}
int main(){try{error_policy();audiences_and_ownership();stale_and_reentrant();live_allocation_isolation();queue_refusal_isolation();std::cout<<"Transfer error handling checks passed\n";}catch(const std::exception& e){fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}}
