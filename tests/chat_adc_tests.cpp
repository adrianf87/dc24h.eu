#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/status.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long allocation_budget=-1;bool fail_once=false; }
void* operator new(std::size_t size) {
    if (allocation_budget==0) {
        if (fail_once) { allocation_budget=-1; }
        throw std::bad_alloc();
    }
    if (allocation_budget>0) { --allocation_budget; }
    if (auto* p=std::malloc(size==0?1:size)) { return p; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
namespace {
using namespace dc24h;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
adc::Message parse(std::string_view value) { return std::get<adc::Message>(adc::parse_line(value)); }
std::vector<std::string> drain(adc::LoginHub& hub,std::uint64_t owner) {
    std::vector<std::string> frames;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) {
        frames.emplace_back(hub.output(owner));hub.consume(owner,frames.back().size());
    }
    return frames;
}
std::string login(adc::LoginHub& hub,std::uint64_t owner,bool complete=true) {
    hub.connect(owner,"127.0.0.1");hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size());const std::string sid(hub.output(owner).substr(5,4));hub.consume(owner,hub.output(owner).size());
    adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)));
    if (complete) { (void)drain(hub,owner); }
    return sid;
}
void pending_and_privacy() {
    adc::LoginHub hub;const auto sid=login(hub,1);login(hub,2,false);drain(hub,1);
    const auto pending=hub.output(2);const auto* pointer=pending.data();
    hub.receive(1,parse("BMSG "+sid+" hello\\sŻółw ME01 XXPrivate PDPrivate"));
    require(drain(hub,1)==std::vector<std::string>{"BMSG "+sid+" hello\\sŻółw ME1\n"},"Public projection lost text/action or leaked fields");
    require(hub.output(2).data()==pointer && hub.output(2)==pending && drain(hub,2).size()==3 && hub.ready(2),"Chat disturbed a pending welcome");
    require(hub.output(2).empty(),"A pre-ready recipient received historical chat");
    hub.receive(1,parse("BMSG "+sid+" private PM"+sid));
    require(drain(hub,1).front().starts_with("ISTA 125 ") && hub.output(2).empty(),"PM-marked broadcast leaked publicly");
    hub.close(2,adc::CloseReason::idle_timeout);const auto closing=hub.output(2);
    hub.receive(1,parse("BMSG "+sid+" alive"));
    require(drain(hub,1)==std::vector<std::string>{"BMSG "+sid+" alive\n"} && hub.output(2)==closing,"Closing recipient received chat");
}
void bounded_slow_recipient() {
    adc::LoginHub hub({4,512,4096});const auto sid=login(hub,1);login(hub,2);login(hub,3);drain(hub,1);drain(hub,2);
    const auto refusal=std::get<std::string>(adc::serialize(adc::make_status(*adc::StatusCode::parse("125"),"Command not implemented","HMSG")));
    while (hub.queued_bytes()+refusal.size()<=512) { hub.receive(2,parse("HMSG Probe")); }
    const auto pinned=hub.output(2);const auto* pointer=pinned.data();
    const auto frame="BMSG "+sid+" "+std::string(128,'x');
    hub.receive(1,parse(frame));
    require(hub.must_close(2) && hub.overloaded(2) && hub.output(2).data()==pointer && hub.output(2)==pinned,
        "Slow recipient bypassed output cap or changed pinned TLS front");
    require(drain(hub,1)==std::vector<std::string>{frame+'\n'} && drain(hub,3)==std::vector<std::string>{frame+'\n'},"Slow peer blocked other recipients");
    hub.disconnect(2,false);hub.disconnect(1,false);hub.disconnect(3,false);require(hub.queued_bytes()==0,"Output charge leaked");
}
void allocation_failures() {
    bool prepared_failure=false,isolated_failure=false,success=false;
    for (long budget=0;budget<160 && !success;++budget) {
        adc::LoginHub hub;const auto sid=login(hub,1);login(hub,2);login(hub,3);drain(hub,1);drain(hub,2);
        const auto frame="BMSG "+sid+" "+std::string(160,'x');const auto message=parse(frame);
        allocation_budget=budget;fail_once=true;
        try { hub.receive(1,message); } catch (...) { allocation_budget=-1;fail_once=false;throw; }
        allocation_budget=-1;fail_once=false;
        unsigned queued=0,closed=0;
        for (const auto owner:{1U,2U,3U}) {
            if (hub.ready(owner)) {
                const auto output=drain(hub,owner);
                require(output.empty() || output==std::vector<std::string>{frame+'\n'},"Partial, duplicate or corrupt fault output");
                if (!output.empty()) { ++queued; }
            } else {
                ++closed;require(hub.must_close(owner) && hub.session_model(owner).terminal_cause()==users::CloseCause::internal_error,
                    "Allocation failure retained usable recipient or changed closure cause");
            }
        }
        prepared_failure=prepared_failure || (closed==1 && queued==0);
        isolated_failure=isolated_failure || (closed==1 && queued==2);
        success=closed==0 && queued==3;
        hub.disconnect(1,false);hub.disconnect(2,false);hub.disconnect(3,false);
        require(hub.users()==0 && hub.queued_bytes()==0,"Fault cleanup retained registry or output");
    }
    require(prepared_failure && isolated_failure && success,"Allocation campaign missed prepublication, isolated fanout or success");
}
}
int main() {
    try { pending_and_privacy();bounded_slow_recipient();allocation_failures();std::cout<<"PASS: Chat ADC adapter readiness, privacy, queue limits, pinned output and allocation isolation\n"; }
    catch (const std::exception& error) { allocation_budget=-1;fail_once=false;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
