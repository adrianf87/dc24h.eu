#include "dc24h/adc/private_chat.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/status.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long allocation_budget=-1;bool fail_once=false; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (allocation_budget==0) { if (fail_once) { allocation_budget=-1; }throw std::bad_alloc(); }
    if (allocation_budget>0) { --allocation_budget; }
    if (auto* p=std::malloc(size==0?1:size)) { return p; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
namespace {
using namespace dc24h;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
adc::Message parse(std::string_view value) { return std::get<adc::Message>(adc::parse_line(value)); }
void add(users::SessionManager& sessions,std::uint64_t owner,std::string_view sid,bool ready=true) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(owner),users::Transport::adc,{})),"Add failed");
    require(!sessions.start_negotiation(id(owner),{}) &&
        !sessions.identify(id(owner),*users::User::create(*users::Identity::create(sid,"CID"+std::to_string(owner),"Nick"+std::to_string(owner))),{}),"Identify failed");
    if (ready) { require(!sessions.mark_ready(id(owner),{}),"Ready failed"); }
}
adc::PrivateDelivery prepare(const adc::PrivateChat& chat,std::string_view body) {
    auto result=chat.prepare(id(1),parse(body));require(std::holds_alternative<adc::PrivateDelivery>(result),"Prepare failed");
    return std::get<adc::PrivateDelivery>(std::move(result));
}
void routing_and_lifetimes() {
    users::SessionManager sessions(8);add(sessions,1,"AAAB");add(sessions,2,"AAAC");add(sessions,3,"AAAD");add(sessions,4,"AAAE",false);
    chat::Core core(sessions);adc::PrivateChat chat(core);
    for (const auto type:{"DMSG","EMSG"}) {
        auto message=prepare(chat,std::string(type)+" AAAB AAAC Private\\sŻółw\\n\\\\path ME01 PMAAAB XXSecret PDSecret");
        const auto expected=std::string(type)+" AAAB AAAC Private\\sŻółw\\n\\\\path PMAAAB ME1\n";
        require(message.frame()->wire()==expected,"Private projection lost text/flags or leaked unknown data");
        unsigned calls=0;const adc::PrivateChat::Sink sink=[&](users::SessionId recipient,const adc::SharedPrivateChatFrame& frame) {
            if ((recipient!=id(1) && recipient!=id(2)) || frame.get()!=message.frame().get()) { std::abort(); }
            if (calls==0 && recipient!=id(2)) { std::abort(); }
            ++calls;return true;
        };
        allocation_budget=0;const auto result=chat.dispatch(message,sink);allocation_budget=-1;
        const auto count=std::string_view(type)=="DMSG" ? 1U : 2U;
        require(std::get<chat::DispatchReport>(result)==chat::DispatchReport{count,0,0} && calls==count,"Directed/echo recipients or allocation-free dispatch differ");
    }
    for (const auto type:{"DMSG","EMSG"}) {
        const auto message=prepare(chat,std::string(type)+" AAAB AAAB Self PMAAAB");unsigned count=0;
        const auto report=chat.dispatch(message,[&](users::SessionId recipient,const adc::SharedPrivateChatFrame&) { require(recipient==id(1),"Self leaked");++count;return true; });
        require(std::get<chat::DispatchReport>(report).queued==1 && count==1,"Self-message duplicated");
    }
    const auto unflagged=prepare(chat,"DMSG AAAB AAAC Plain ME0");
    require(unflagged.frame()->wire()=="DMSG AAAB AAAC Plain ME0\n","Absent PM was fabricated");
    for (const auto target:{"AAAE","ZZZZ"}) {
        require(std::get<adc::PrivateChatError>(chat.prepare(id(1),parse(std::string("EMSG AAAB ")+target+" Hidden PMAAAB")))==adc::PrivateChatError::recipient_unavailable,"Pending/missing target accepted");
    }
    require(std::get<adc::PrivateChatError>(chat.prepare(id(3),parse("DMSG AAAB AAAC Text")))==adc::PrivateChatError::wrong_source,"Spoofed source accepted");
    require(std::get<adc::PrivateChatError>(chat.prepare(id(1),parse("EMSG AAAB AAAC Group PMAAAD")))==adc::PrivateChatError::unsupported_group,"Group origin impersonated");
    require(std::get<adc::PrivateChatError>(chat.prepare(id(1),parse("BMSG AAAB Text")))==adc::PrivateChatError::unsupported_type,"Public mode became private");
    auto old=prepare(chat,"EMSG AAAB AAAC Old PMAAAB");
    require(!sessions.finish_close(id(2),users::CloseCause::peer_closed,{}) && !sessions.remove(id(2)),"Target cleanup failed");
    add(sessions,5,"AAAC");unsigned calls=0;
    const adc::PrivateChat::Sink sink=[&](users::SessionId,const adc::SharedPrivateChatFrame&) { ++calls;return true; };
    require(std::get<chat::Error>(chat.dispatch(old,sink))==chat::Error::recipient_unavailable && calls==0,"Stale SID delivered to a replacement or emitted echo");
    auto current=prepare(chat,"EMSG AAAB AAAC Current PMAAAB");
    require(current.admission().target()==id(5),"New message did not bind the new lifetime");
    auto moved=std::move(current);
    require(std::get<chat::Error>(chat.dispatch(current,sink))==chat::Error::invalid_text,"Moved-from private plan accepted");
    require(!sessions.rename(id(5),"Renamed",{}) && std::get<chat::DispatchReport>(chat.dispatch(moved,sink)).queued==2,"Rename invalidated a live target");
    require(!sessions.begin_close(id(1),users::CloseCause::shutdown,{}),"Sender close failed");calls=0;
    require(std::get<chat::Error>(chat.dispatch(moved,sink))==chat::Error::sender_not_ready && calls==0,"Closing sender published");
}
void bounds_and_preparation_faults() {
    users::SessionManager sessions(2);add(sessions,1,"AAAB");add(sessions,2,"AAAC");chat::Core core(sessions);adc::PrivateChat chat(core);
    auto exact=parse("DMSG AAAB AAAC "+std::string(adc::max_frame_bytes-15-7,'x')+" PMAAAB");
    require(std::get<adc::PrivateDelivery>(chat.prepare(id(1),exact)).frame()->wire().size()==adc::max_frame_bytes+1,"Exact private frame rejected");
    exact.parameters[0]+='x';require(std::get<adc::PrivateChatError>(chat.prepare(id(1),exact))==adc::PrivateChatError::invalid_message,"Oversize private frame accepted");
    auto input=parse("EMSG AAAB AAAC "+std::string(160,'x')+" PMAAAB ME1 XXsecret");
    const auto before=sessions.statistics();bool failed=false,success=false;
    for (long budget=0;budget<100 && !success;++budget) {
        allocation_budget=budget;
        try { auto result=chat.prepare(id(1),input);allocation_budget=-1;require(std::holds_alternative<adc::PrivateDelivery>(result),"Typed fault rejection");success=true; }
        catch (const std::bad_alloc&) { allocation_budget=-1;failed=true; }
        require(sessions.statistics()==before && sessions.find(id(1))->ready() && sessions.find(id(2))->ready(),"Preparation fault mutated sessions");
    }
    require(failed && success,"Preparation campaign missed boundary");
}
std::vector<std::string> drain(adc::LoginHub& hub,std::uint64_t owner) {
    std::vector<std::string> frames;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) { frames.emplace_back(hub.output(owner));hub.consume(owner,frames.back().size()); }
    return frames;
}
std::string login(adc::LoginHub& hub,std::uint64_t owner,bool complete=true) {
    hub.connect(owner,"127.0.0.1");hub.receive(owner,parse("HSUP ADBASE ADTIGR"));hub.consume(owner,hub.output(owner).size());
    const std::string sid(hub.output(owner).substr(5,4));hub.consume(owner,hub.output(owner).size());
    adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)));
    if (complete) { drain(hub,owner); }return sid;
}
void queues_and_pending() {
    adc::LoginHub hub({4,512,4096});const auto source=login(hub,1),target=login(hub,2);login(hub,3);drain(hub,1);drain(hub,2);
    const auto frame="EMSG "+source+" "+target+" Secret PM"+source;
    hub.receive(1,parse(frame));require(hub.output(1).data()==hub.output(2).data() && hub.output(3).empty(),"Echo storage differs or observer received private text");
    hub.consume(1,4);const auto full=std::string(hub.output(2));hub.disconnect(1,false);
    require(drain(hub,2)==std::vector<std::string>{full} && hub.output(3).empty(),"Source cleanup invalidated recipient private frame");
    const auto new_source=login(hub,4);drain(hub,2);drain(hub,3);
    const auto refusal=std::get<std::string>(adc::serialize(adc::make_status(*adc::StatusCode::parse("125"),"Command not implemented","HMSG")));
    while (hub.queued_bytes()+refusal.size()<=512) { hub.receive(2,parse("HMSG Probe")); }
    const auto pinned=hub.output(2);const auto* pointer=pinned.data();
    const auto large="EMSG "+new_source+" "+target+" "+std::string(128,'x')+" PM"+new_source;
    hub.receive(4,parse(large));
    require(hub.overloaded(2) && hub.output(2).data()==pointer && hub.output(2)==pinned &&
        drain(hub,4)==std::vector<std::string>{large+'\n'} && hub.output(3).empty(),"Slow target broke isolation, echo or pinned buffer");
    hub.disconnect(2,false);hub.disconnect(3,false);hub.disconnect(4,false);require(hub.queued_bytes()==0,"Private charge leaked");
    adc::LoginHub pending;const auto sender=login(pending,1),unready=login(pending,2,false);drain(pending,1);
    const auto front=pending.output(2);const auto* front_pointer=front.data();
    pending.receive(1,parse("EMSG "+sender+" "+unready+" NotReady PM"+sender));
    require(drain(pending,1).front().starts_with("ISTA 140 ") && pending.output(2).data()==front_pointer &&
        pending.output(2)==front,"Pending target received private data or changed welcome");
    drain(pending,2);require(pending.output(2).empty(),"Private backlog released after welcome");
}
void enqueue_faults() {
    bool preparation=false,isolated=false,success=false;
    for (unsigned warmup=0;warmup<4 && !(preparation && isolated && success);++warmup) {
        for (long budget=0;budget<180;++budget) {
            adc::LoginHub hub;const auto source=login(hub,1),target=login(hub,2);login(hub,3);drain(hub,1);drain(hub,2);
            for (unsigned n=0;n<warmup;++n) { hub.receive(2,parse("HMSG Warmup"));drain(hub,2); }
            const auto body="EMSG "+source+" "+target+" "+std::string(160,'x')+" PM"+source;
            const auto input=parse(body);allocation_budget=budget;fail_once=true;
            hub.receive(1,input);allocation_budget=-1;fail_once=false;
            unsigned queued=0,closed=0;
            for (const auto owner:{1U,2U}) {
                if (hub.ready(owner)) { const auto output=drain(hub,owner);require(output.empty() || output==std::vector<std::string>{body+'\n'},"Corrupt/duplicate private fault output");if (!output.empty()) { ++queued; } }
                else { ++closed;require(hub.must_close(owner) && hub.session_model(owner).terminal_cause()==users::CloseCause::internal_error,"Fault retained usable private peer"); }
            }
            require(hub.ready(3) && hub.output(3).empty(),"Private fault leaked to unrelated observer");
            preparation=preparation || (closed==1 && queued==0);isolated=isolated || (closed==1 && queued==1);
            const bool done=closed==0 && queued==2;success=success || done;
            for (const auto owner:{1U,2U,3U}) { hub.disconnect(owner,false); }
            require(hub.queued_bytes()==0 && hub.users()==0,"Fault cleanup leaked private resources");
            if (done) { break; }
        }
    }
    require(preparation && isolated && success,"Private fault campaign missed preparation, queue refusal or success");
}
}
int main() {
    try { routing_and_lifetimes();bounds_and_preparation_faults();queues_and_pending();enqueue_faults();std::cout<<"PASS: Private Chat D/E routing, lifetime privacy, shared queues, wire bounds and allocation isolation\n"; }
    catch (const std::exception& error) { allocation_budget=-1;fail_once=false;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
