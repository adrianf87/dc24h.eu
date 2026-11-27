#include "dc24h/adc/public_chat.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace { long allocation_budget=-1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (allocation_budget==0) { throw std::bad_alloc(); }
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
adc::Message parse(std::string_view text) { return std::get<adc::Message>(adc::parse_line(text)); }
void add(users::SessionManager& sessions,std::uint64_t owner,bool ready=true) {
    const auto sid=adc::Sid::from_value(static_cast<std::uint32_t>(owner))->str();
    require(std::holds_alternative<const users::Session*>(sessions.add(id(owner),users::Transport::adc,{})),"Add failed");
    require(!sessions.start_negotiation(id(owner),{}) &&
        !sessions.identify(id(owner),*users::User::create(*users::Identity::create(sid,"CID"+std::to_string(owner),"Nick"+std::to_string(owner))),{}),"Identify failed");
    if (ready) { require(!sessions.mark_ready(id(owner),{}),"Ready failed"); }
}
adc::PublicBroadcast prepared(const adc::PublicChat& chat,const adc::Message& message) {
    auto result=chat.prepare(id(1),message);require(std::holds_alternative<adc::PublicBroadcast>(result),"Prepare failed");
    return std::get<adc::PublicBroadcast>(std::move(result));
}
void ownership_and_policy() {
    static_assert(!std::is_copy_constructible_v<adc::PublicChatFrame>);
    users::SessionManager sessions(400);for (std::uint64_t owner=1;owner<=270;++owner) { add(sessions,owner); }
    add(sessions,271,false);chat::Core core(sessions);adc::PublicChat chat(core);
    auto input=parse("BMSG AAAB Public\\sŻółw\\n\\\\path ME01 XXPrivate PDPrivate XX");
    auto message=prepared(chat,input);input.parameters.clear();
    const auto& frame=message.frame();
    auto copy=message;
    auto moved=std::move(copy);
    require(moved.frame().get()==frame.get() && !copy.frame(),"Plan move lost immutable ownership");
    const adc::PublicChat::Sink unused=[](users::SessionId,const adc::SharedPublicChatFrame&) { return true; };
    require(std::get<chat::Error>(chat.dispatch(copy,unused))==chat::Error::invalid_text,"Moved-from plan accepted");
    require(frame->wire()=="BMSG AAAB Public\\sŻółw\\n\\\\path ME1\n" &&
        frame->message().parameters==std::vector<std::string>{"Public Żółw\n\\path","ME1"} &&
        message.admission().recipients().size()==270,"Public projection lost text or leaked fields");
    unsigned recipients=0;const char* shared=frame->wire().data();
    const adc::PublicChat::Sink sink=[&](users::SessionId,const adc::SharedPublicChatFrame& value) {
        if (value.get()!=frame.get() || value->wire().data()!=shared) { std::abort(); }
        ++recipients;return true;
    };
    allocation_budget=0;const auto report=chat.dispatch(message,sink);allocation_budget=-1;
    require(std::get<chat::DispatchReport>(report)==chat::DispatchReport{270,0,0} && recipients==270,"Shared dispatch copied/allocated frames or omitted sender");
    require(std::get<chat::Error>(chat.dispatch(message,{}))==chat::Error::missing_sink,"Missing sink accepted");
    for (const char* body:{"HMSG text","DMSG AAAB AAAC text PMAAAB","EMSG AAAB AAAC text PMAAAB","FMSG AAAB +BASE text","IMSG text"}) {
        require(std::get<adc::PublicChatError>(chat.prepare(id(1),parse(body)))==adc::PublicChatError::unsupported_type,"Other mode became public");
    }
    require(std::get<adc::PublicChatError>(chat.prepare(id(1),parse("BMSG AAAB text PMAAAB")))==adc::PublicChatError::private_message,"PM-marked broadcast became public");
    require(std::get<adc::PublicChatError>(chat.prepare(id(1),parse("BMSG AAAB text ME2")))==adc::PublicChatError::invalid_message,"Invalid flag accepted");
    require(std::get<adc::PublicChatError>(chat.prepare(id(2),parse("BMSG AAAB text")))==adc::PublicChatError::wrong_source,"Forged source accepted");
    require(std::get<adc::PublicChatError>(chat.prepare(id(271),parse("BMSG AAIP text")))==adc::PublicChatError::invalid_state,"Pending source accepted");
    require(!sessions.finish_close(id(1),users::CloseCause::peer_closed,{}) && !sessions.remove(id(1)),"Removal failed");
    recipients=0;
    require(std::get<chat::Error>(chat.dispatch(message,sink))==chat::Error::missing_sender && recipients==0 &&
        frame->wire().data()==shared,"Removed sender published or invalidated owned wire");
}
void bounds_and_faults() {
    users::SessionManager sessions(2);add(sessions,1);add(sessions,2);chat::Core core(sessions);adc::PublicChat chat(core);
    const auto exact=parse("BMSG AAAB "+std::string(adc::max_frame_bytes-10,'x'));
    require(prepared(chat,exact).frame()->wire().size()==adc::max_frame_bytes+1,"Exact encoded frame rejected");
    const auto escaped=parse("BMSG AAAB "+std::string((adc::max_frame_bytes-10)/2,'x'));
    auto decoded=escaped;decoded.parameters[0]=std::string((adc::max_frame_bytes-10)/2,' ');
    require(prepared(chat,decoded).frame()->wire().size()==adc::max_frame_bytes+1,"Escaped wire bound differs");
    decoded.parameters[0]+=' ';
    require(std::get<adc::PublicChatError>(chat.prepare(id(1),decoded))==adc::PublicChatError::invalid_message,"Oversize hand-built frame accepted");
    const auto input=parse("BMSG AAAB "+std::string(256,'x')+" XXprivate");
    const auto before=sessions.statistics();bool failed=false,success=false;
    for (long budget=0;budget<80 && !success;++budget) {
        allocation_budget=budget;
        try { auto result=chat.prepare(id(1),input);allocation_budget=-1;
            require(std::holds_alternative<adc::PublicBroadcast>(result),"Unexpected typed fault result");success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1;failed=true; }
        require(sessions.statistics()==before && sessions.find(id(1))->ready() && sessions.find(id(2))->ready(),"Preparation fault changed models");
    }
    require(failed && success,"Preparation campaign missed allocation boundary");
}
std::vector<std::string> drain(adc::LoginHub& hub,std::uint64_t owner) {
    std::vector<std::string> output;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) {
        output.emplace_back(hub.output(owner));hub.consume(owner,output.back().size());
    }
    return output;
}
std::string login(adc::LoginHub& hub,std::uint64_t owner) {
    hub.connect(owner,"127.0.0.1");hub.receive(owner,parse("HSUP ADBASE ADTIGR"));hub.consume(owner,hub.output(owner).size());
    const std::string sid(hub.output(owner).substr(5,4));hub.consume(owner,hub.output(owner).size());
    adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)));
    drain(hub,owner);return sid;
}
void shared_queue_lifetimes() {
    adc::LoginHub hub;const auto source=login(hub,1),second=login(hub,2);login(hub,3);drain(hub,1);drain(hub,2);
    const auto body="BMSG "+source+" "+std::string(200,'x');hub.receive(1,parse(body));
    const auto one=hub.output(1),two=hub.output(2),three=hub.output(3);
    require(one==body+'\n' && one.data()==two.data() && one.data()==three.data(),"Queues do not share public frame storage");
    const auto full_charge=hub.queued_bytes();hub.consume(1,5);
    require(hub.queued_bytes()==full_charge && hub.output(1).data()==one.data()+5 && hub.output(2).data()==two.data(),"Independent offsets/charges differ");
    hub.disconnect(1,false);require(hub.output(2)==two && hub.output(3)==three,"Source cleanup released another queue's wire");
    hub.consume(2,3);const auto pinned=hub.output(2);const auto* pointer=pinned.data();
    hub.receive(2,parse("BMSG "+second+" Bad ME2"));
    require(hub.closing(2) && hub.output(2).data()==pointer && hub.output(2)==pinned && hub.output(3)==three,"Terminal append mutated shared pinned front");
    require(drain(hub,3)==std::vector<std::string>{body+'\n'},"Independent recipient failed after source/peer close");
    const std::string suffix(two.substr(3));
    const auto terminal=drain(hub,2);
    require(terminal.size()==3 && terminal[0]==suffix && terminal[1].starts_with("ISTA 240 ") &&
        terminal[2].starts_with("IQUI "+second+" "),"Pinned suffix/terminal order differs");
    hub.disconnect(2,false);hub.disconnect(3,false);require(hub.queued_bytes()==0,"Logical output charge leaked");
}
}
int main() {
    try { ownership_and_policy();bounds_and_faults();shared_queue_lifetimes();
        std::cout<<"PASS: Public Chat policy, shared frame ownership, wire bounds, lifecycle and allocation rollback\n";
    } catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
