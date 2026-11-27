#include "dc24h/chat/core.hpp"
#include "dc24h/text/public_text.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long allocation_budget=-1; }
void* operator new(std::size_t size) {
    if (allocation_budget==0) { throw std::bad_alloc(); }
    if (allocation_budget>0) { --allocation_budget; }
    if (auto* p=std::malloc(size==0?1:size)) { return p; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
namespace {
using namespace dc24h;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
void add(users::SessionManager& sessions,std::uint64_t value,int phase=3) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(value),users::Transport::adc,{})),"Add failed");
    if (phase<1) { return; }
    require(!sessions.start_negotiation(id(value),{}),"Negotiation failed");
    if (phase<2) { return; }
    const auto suffix=std::to_string(value);
    auto user=*users::User::create(*users::Identity::create("SID"+suffix,"CID"+suffix,"Nick"+suffix));
    require(!sessions.identify(id(value),std::move(user),{}),"Identify failed");
    if (phase>=3) { require(!sessions.mark_ready(id(value),{}),"Ready failed"); }
}
chat::Broadcast prepare(const chat::Core& core,std::string_view text="Hello Żółw\n\\path") {
    auto result=core.prepare(id(1),"SID1",text,true);
    require(std::holds_alternative<chat::Broadcast>(result),"Prepare failed");
    return std::get<chat::Broadcast>(std::move(result));
}
void lifecycle_and_dispatch() {
    users::SessionManager sessions(400);chat::Core core(sessions);
    require(std::get<chat::Error>(core.prepare(id(1),"SID1","hi"))==chat::Error::missing_sender,"Missing sender accepted");
    add(sessions,1,0);
    require(std::get<chat::Error>(core.prepare(id(1),"SID1","hi"))==chat::Error::sender_not_ready,"Unready sender accepted");
    require(!sessions.start_negotiation(id(1),{}) &&
        !sessions.identify(id(1),*users::User::create(*users::Identity::create("SID1","CID1","Nick1")),{}) &&
        !sessions.mark_ready(id(1),{}),"Sender fixture failed");
    for (std::uint64_t value=2;value<=270;++value) { add(sessions,value); }
    add(sessions,271,2);add(sessions,272,1);add(sessions,273,0);add(sessions,274);
    require(!sessions.begin_close(id(274),users::CloseCause::shutdown,{}),"Closing fixture failed");
    auto message=prepare(core);
    require(message.sender()==id(1) && message.source()=="SID1" && message.text()=="Hello Żółw\n\\path" &&
        message.third_person()==true && message.recipients().size()==270 && message.recipients().front()==id(1) &&
        message.recipients().back()==id(270),"Ordered multipage membership includes unready/closing models");
    // Plans are snapshots; later joins are excluded and removed lifetime IDs
    // cannot select a different user even after protocol SID reuse.
    require(!sessions.finish_close(id(2),users::CloseCause::peer_closed,{}) && !sessions.remove(id(2)),"Removal failed");
    require(!sessions.begin_close(id(3),users::CloseCause::peer_closed,{}),"Close failed");
    add(sessions,275);
    std::vector<std::uint64_t> delivered;
    const auto result=core.dispatch(message,[&](users::SessionId recipient,const chat::Broadcast& body) {
        require(body.text()==message.text(),"Message changed during dispatch");
        delivered.push_back(recipient.value());return recipient!=id(4);
    });
    require(std::get<chat::DispatchReport>(result)==chat::DispatchReport{267,2,1} && delivered.size()==268 &&
        delivered.front()==1 && delivered.back()==270,"Late lifecycle changes, refusal isolation or sender echo differ");
    require(!sessions.rename(id(1),"Renamed",{}),"Rename failed");
    unsigned queued=0;const chat::Core::Sink sink=[&](users::SessionId,const chat::Broadcast&) { ++queued;return true; };
    require(std::holds_alternative<chat::DispatchReport>(core.dispatch(message,sink)) && queued==268,"Public source lost after rename");
    require(std::get<chat::Error>(core.dispatch(message,{}))==chat::Error::missing_sink,"Missing sink accepted");
    require(!sessions.finish_close(id(1),users::CloseCause::shutdown,{}),"Sender cleanup failed");queued=0;
    require(std::get<chat::Error>(core.dispatch(message,sink))==chat::Error::sender_not_ready && queued==0,"Stale sender published");
    require(!sessions.remove(id(1)),"Sender remove failed");
    require(std::get<chat::Error>(core.dispatch(message,sink))==chat::Error::missing_sender,"Removed sender published");
    require(message.text()=="Hello Żółw\n\\path" && message.source()=="SID1","Plan aliases removed source");
}
void validation_and_faults() {
    users::SessionManager sessions(4);add(sessions,1);add(sessions,2);chat::Core core(sessions);
    require(std::get<chat::Error>(core.prepare(id(1),"SID2","hi"))==chat::Error::wrong_source,"Spoofed source accepted");
    for (const auto& text:std::vector<std::string>{"", "bad\ttext", "bad\rtext",std::string("bad\0text",8),"e\xcc\x81",std::string(1,char(0xff)),
            std::string(text::max_chat_text_bytes+1,'x')}) {
        require(std::get<chat::Error>(core.prepare(id(1),"SID1",text))==chat::Error::invalid_text,"Invalid chat text accepted");
    }
    require(prepare(core,std::string(text::max_chat_text_bytes,'x')).text().size()==text::max_chat_text_bytes,"Exact decoded cap rejected");
    require(prepare(core," ").text()==" ","Whitespace text trimmed");
    auto candidate=std::string(128,'x');auto before=sessions.statistics();bool failed=false,success=false;
    for (long budget=0;budget<20 && !success;++budget) {
        allocation_budget=budget;
        try { auto result=core.prepare(id(1),"SID1",candidate);allocation_budget=-1;
            require(std::holds_alternative<chat::Broadcast>(result),"Fault campaign typed refusal");success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1;failed=true; }
        require(sessions.statistics()==before && sessions.by_sid("SID1") && sessions.by_sid("SID2"),"Preparation failure mutated models");
    }
    require(failed && success,"Preparation campaign missed failure or success");
    const auto message=prepare(core);unsigned count=0;
    const chat::Core::Sink sink=[&](users::SessionId,const chat::Broadcast&) { ++count;return true; };
    allocation_budget=0;const auto result=core.dispatch(message,sink);allocation_budget=-1;
    require(std::get<chat::DispatchReport>(result)==chat::DispatchReport{2,0,0} && count==2,"Dispatch allocated or duplicated delivery");
}
}
int main() {
    try { lifecycle_and_dispatch();validation_and_faults();std::cout<<"PASS: Chat Core ownership, text/source gates, lifecycle routing, multipage snapshots and allocation isolation\n"; }
    catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
