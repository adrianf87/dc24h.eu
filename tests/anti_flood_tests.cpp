#include "dc24h/chat/anti_flood.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace { bool deny_allocations=false; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (deny_allocations) { throw std::bad_alloc(); }
    if (auto* value=std::malloc(size == 0 ? 1 : size)) { return value; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete(void* value,std::size_t) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value,std::size_t) noexcept { std::free(value); }
namespace {
using namespace dc24h;
using namespace std::chrono_literals;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
void buckets_and_bounds() {
    const auto start=chat::FloodClock::time_point{};
    chat::FloodGuard rate({5,2,32768,131072});
    require(rate.admit(10,start).allowed && rate.admit(10,start).allowed, "Initial burst missing");
    require(rate.admit(10,start) == chat::FloodDecision{false,true} && rate.admit(10,start) == chat::FloodDecision{false,false}, "Denial/notice not bounded");
    require(!rate.admit(10,start+200ms-1ns).allowed && rate.admit(10,start+200ms).allowed, "Fractional message refill boundary differs");
    chat::FloodGuard notice({1,1,32768,131072});
    require(notice.admit(1,start).allowed && notice.admit(1,start).notify &&
        !notice.admit(1,start+1s-1ns).notify && notice.admit(1,start+1s).allowed && notice.admit(1,start+1s).notify, "Success reset notice clock or exact notice boundary differs");
    chat::FloodGuard atomic_bytes({1,2,1,16384});
    require(atomic_bytes.admit(16000,start).allowed && !atomic_bytes.admit(385,start).allowed &&
        atomic_bytes.admit(384,start).allowed, "Byte denial partially consumed message budget");
    chat::FloodGuard atomic_messages({1,1,1,16384});
    require(atomic_messages.admit(10000,start).allowed && !atomic_messages.admit(5000,start).allowed &&
        atomic_messages.admit(6385,start+1s).allowed, "Message denial partially consumed byte budget");
    chat::FloodGuard byte_rate({100,10,1024,16384});
    require(byte_rate.admit(16384,start).allowed && !byte_rate.admit(1,start+976562ns).allowed &&
        byte_rate.admit(1,start+976563ns).allowed, "Fractional byte refill rounded or lost credit");
    chat::FloodGuard extremes({1,1,1,16384});
    require(extremes.admit(0,chat::FloodClock::time_point::min()).allowed &&
        !extremes.admit(std::numeric_limits<std::size_t>::max(),chat::FloodClock::time_point::min()).allowed &&
        extremes.admit(16384,chat::FloodClock::time_point::max()).allowed &&
        extremes.admit(1,chat::FloodClock::time_point::min()) == chat::FloodDecision{false,true} &&
        extremes.admit(1,chat::FloodClock::time_point::min()) == chat::FloodDecision{false,false}, "Extreme/backwards clock or size overflow");
    chat::FloodGuard bounded;
    std::size_t accepted=0,notifications=0;
    deny_allocations=true;
    for (unsigned i=0; i<10000; ++i) { const auto decision=bounded.admit(1,start); accepted+=decision.allowed; notifications+=decision.notify; }
    deny_allocations=false;
    require(accepted == 10 && notifications == 1, "Guard allocates, grows state or amplifies flood replies");
    for (int field=0; field<4; ++field) {
        for (const auto value : {std::uint32_t{0},std::numeric_limits<std::uint32_t>::max()}) {
            chat::FloodLimits limits;
            switch (field) { case 0: limits.messages_per_second=value; break; case 1: limits.message_burst=value; break;
            case 2: limits.bytes_per_second=value; break; default: limits.byte_burst=value; break; }
            bool rejected=false; try { chat::FloodGuard guard(limits); } catch (const std::invalid_argument&) { rejected=true; }
            require(rejected, "Invalid quota accepted");
        }
    }
    auto limits=chat::FloodLimits{}; limits.byte_burst=16383;
    bool rejected=false; try { chat::validate(limits); } catch (const std::invalid_argument&) { rejected=true; }
    require(rejected, "Byte burst cannot carry one legal full frame");
    chat::validate({chat::max_message_rate,chat::max_message_rate,chat::max_byte_rate,chat::max_byte_rate});
}
adc::Message parse(std::string_view value) { return std::get<adc::Message>(adc::parse_line(value)); }
std::string drain(adc::LoginHub& hub,std::uint64_t owner) {
    std::string result;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) { const std::string wire(hub.output(owner)); result+=wire; hub.consume(owner,wire.size()); }
    return result;
}
std::string join(adc::LoginHub& hub,std::uint64_t owner) {
    hub.connect(owner,"127.0.0.1"); hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size()); const std::string sid(hub.output(owner).substr(5,4)); hub.consume(owner,hub.output(owner).size());
    adc::TigerDigest pid{}; pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)));
    (void)drain(hub,owner); return sid;
}
void live_adapter() {
    adc::LoginLimits limits; limits.anti_flood=chat::FloodLimits{1,2,32768,131072}; adc::LoginHub hub(limits);
    const auto source=join(hub,1), target=join(hub,2); (void)drain(hub,1);
    const auto now=chat::FloodClock::time_point{};
    const auto before=hub.sessions().statistics();
    hub.receive(1,parse("BMSG "+source+" Public ME1"),now);
    require(drain(hub,1) == "BMSG "+source+" Public ME1\n" && drain(hub,2) == "BMSG "+source+" Public ME1\n", "First burst lost");
    hub.receive(1,parse("DMSG "+source+" "+target+" Private PM"+source),now);
    require(hub.output(1).empty() && drain(hub,2).starts_with("DMSG "+source), "Directed budget routing differs");
    hub.receive(1,parse("EMSG "+source+" "+target+" FloodSecret PM"+source+" ME1"),now);
    const auto refusal=drain(hub,1);
    require(refusal == "ISTA 100 Chat\\smessage\\srate\\slimit\\sexceeded FCEMSG\n" && hub.output(2).empty() &&
        hub.ready(1) && hub.sessions().statistics() == before, "Denied private message leaked or closed source");
    for (int i=0; i<100; ++i) { hub.receive(1,parse("BMSG "+source+" FloodSecret"),now); }
    require(hub.output(1).empty() && hub.output(2).empty(), "Repeated denial amplified output");
    hub.receive(2,parse("BMSG "+target+" Independent"),now);
    (void)drain(hub,1); require(drain(hub,2).find("Independent") != std::string::npos, "Another source inherited quota");
    require(std::get<chat::DispatchReport>(hub.send_hub_message("System bypasses client quota")).queued == 2, "System message consumed client budget");
    (void)drain(hub,1); (void)drain(hub,2);
    hub.receive(1,parse("BMSG "+source+" Recovered"),now+1s);
    require(drain(hub,1) == "BMSG "+source+" Recovered\n" && drain(hub,2) == "BMSG "+source+" Recovered\n", "Refill could not resume NORMAL chat");
    hub.receive(1,parse("BMSG "+target+" Forged"),now+1s);
    const auto terminal=drain(hub,1);
    require(terminal.starts_with("ISTA 240 ") && terminal.find("Forged") == std::string::npos && hub.output(2).empty(), "Exhaustion masked fatal source validation");
    hub.disconnect(1); (void)drain(hub,2); hub.disconnect(2,false); require(hub.queued_bytes() == 0, "Flood cleanup leaked charges");
}
}
int main() {
    try { buckets_and_bounds(); live_adapter(); std::cout << "PASS: bounded anti-flood buckets, fractional refill, denial suppression, protocol precedence and private isolation\n"; }
    catch (const std::exception& error) { deny_allocations=false; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
