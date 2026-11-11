#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
namespace { bool fail_allocations=false; }
void* operator new(std::size_t size) {
    if (fail_allocations) { throw std::bad_alloc(); }
    if (auto* memory=std::malloc(size==0?1:size)) { return memory; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory,std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory,std::size_t) noexcept { std::free(memory); }

namespace {
using namespace dc24h;
using namespace dc24h::adc;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
Message parse(std::string_view value) { return std::get<Message>(parse_line(value)); }
std::string drain(LoginHub& hub,std::uint64_t owner) {
    std::string result;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) { const std::string wire(hub.output(owner)); result+=wire; hub.consume(owner,wire.size()); }
    return result;
}
std::string join(LoginHub& hub,std::uint64_t owner,std::string& welcome) {
    hub.connect(owner,"127.0.0.1"); hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size()); const std::string sid(hub.output(owner).substr(5,4)); hub.consume(owner,hub.output(owner).size());
    TigerDigest pid{}; pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)));
    welcome=drain(hub,owner); require(hub.ready(owner),"Configured welcome prevented readiness"); return sid;
}

void saturation() {
    chat::Statistics stats;
    fail_allocations=true;
    { chat::Attempt a(&stats); }
    { chat::Attempt a(&stats); a.dispatched(true,3,1,2); }
    { chat::Attempt a(&stats); a.outcome(chat::Event::flood_limited); }
    { chat::Attempt a(nullptr); a.outcome(chat::Event::internal_error); }
    fail_allocations=false;
    require(stats.values==std::array<std::uint64_t,9>{3,1,0,1,1,0,3,1,2},"Attempt accounting or no-allocation contract differs");
    stats.values.fill(std::numeric_limits<std::uint64_t>::max()-1);
    fail_allocations=true;
    { chat::Attempt a(&stats); a.dispatched(false,5,5,5); }
    { chat::Attempt a(&stats); a.dispatched(false,5,5,5); }
    fail_allocations=false;
    require(stats.values[0]==UINT64_MAX && stats.values[2]==UINT64_MAX && stats.values[6]==UINT64_MAX &&
        stats.values[7]==UINT64_MAX && stats.values[8]==UINT64_MAX,"Chat totals wrapped");
}
void lifecycle() {
    LoginHub hub; std::string welcome;
    const auto source=join(hub,1,welcome), target=join(hub,2,welcome); (void)drain(hub,1);
    require(hub.chat_statistics()==chat::Statistics{},"Login/system welcome counted as client chat");
    hub.receive(1,parse("BMSG "+source+" Secret ME1"));
    hub.receive(1,parse("DMSG "+source+" "+target+" Secret PM"+source));
    hub.receive(1,parse("EMSG "+source+" "+target+" Secret PM"+source));
    hub.receive(1,parse("EMSG "+source+" "+source+" Secret PM"+source));
    hub.receive(1,parse("DMSG "+source+" ZZZZ Secret"));
    require(hub.chat_statistics().values==std::array<std::uint64_t,9>{5,1,3,1,0,0,6,0,0},"Public/direct/echo/self or unavailable counters differ");
    (void)drain(hub,1); (void)drain(hub,2);
    const auto before=hub.chat_statistics();
    (void)hub.send_hub_message("System Secret");
    require(hub.chat_statistics()==before,"Trusted IMSG counted as client chat");
    hub.receive(1,parse("BMSG "+target+" ForgedSecret"));
    require(hub.chat_statistics().values[0]==6 && hub.chat_statistics().values[3]==2,"Forged MSG missing rejection");
    const auto closed=hub.chat_statistics(); hub.receive(1,parse("BMSG "+source+" ClosedSecret"));
    hub.disconnect(1,false); hub.disconnect(2,false);
    require(hub.chat_statistics()==closed && LoginHub{}.chat_statistics()==chat::Statistics{},"Cleanup changed totals or counters leaked between hubs");
}
void policy_flood_and_faults() {
    LoginLimits limits; limits.chat.public_enabled=false; limits.anti_flood=chat::FloodLimits{1,1,32768,131072};
    LoginHub hub(limits); std::string welcome; const auto sid=join(hub,1,welcome);
    const auto now=chat::FloodClock::now(); const auto message=parse("BMSG "+sid+" Secret");
    hub.receive(1,message,now);
    for (int i=0;i<20;++i) { hub.receive(1,message,now); }
    require(hub.chat_statistics().values==std::array<std::uint64_t,9>{21,0,0,1,20,0,0,0,0},"Flood suppression changed accounting or policy denial counted dispatch");
    LoginHub allocation; const auto owner=join(allocation,1,welcome); const auto candidate=parse("BMSG "+owner+" AllocationSecret");
    fail_allocations=true; allocation.receive(1,candidate); const auto fault=allocation.chat_statistics(); fail_allocations=false;
    require(fault.values==std::array<std::uint64_t,9>{1,0,0,0,0,1,0,0,0},"Failed preparation changed dispatch totals or lost error");
    LoginLimits small; small.output_per_client=512; LoginHub queues(small);
    const auto source=join(queues,1,welcome); (void)join(queues,2,welcome); (void)drain(queues,1);
    queues.receive(1,parse("BMSG "+source+" "+std::string(1000,'x')));
    require(queues.chat_statistics().values==std::array<std::uint64_t,9>{1,1,0,0,0,0,0,0,2},"Queue refusal counted transport delivery or internal failure");
}
}
int main() {
    try { saturation(); lifecycle(); policy_flood_and_faults(); std::cout<<"PASS: bounded private chat telemetry, exact outcomes and failure accounting\n"; }
    catch (const std::exception& e) { fail_allocations=false; std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
