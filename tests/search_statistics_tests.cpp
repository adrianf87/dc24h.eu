#include "dc24h/search/statistics.hpp"
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
    search::Statistics statistics;
    fail_allocations=true;
    {search::Attempt attempt(&statistics,search::Kind::query);attempt.outcome(search::Outcome::routed);attempt.delivery(3,1,2,4);}
    {search::Attempt attempt(&statistics,search::Kind::query);attempt.outcome(search::Outcome::ignored);}
    {search::Attempt attempt(&statistics,search::Kind::result);attempt.outcome(search::Outcome::recipient_limited);attempt.delivery(0,0,1);}
    {search::Attempt attempt(nullptr,search::Kind::result);}
    fail_allocations=false;
    require(statistics.values==std::array<std::uint64_t,19>{2,1,0,0,1,0,3,1,2,4,1,0,0,0,1,0,0,0,1},"Attempt outcome/delivery accounting allocated or differs");
    statistics.values.fill(UINT64_MAX-1);
    fail_allocations=true;
    {search::Attempt attempt(&statistics,search::Kind::result);attempt.outcome(search::Outcome::routed);attempt.delivery(5,5,5);}
    {search::Attempt attempt(&statistics,search::Kind::result);attempt.outcome(search::Outcome::routed);attempt.delivery(5,5,5);}
    fail_allocations=false;
    require(statistics.values[10]==UINT64_MAX && statistics.values[11]==UINT64_MAX && statistics.values[16]==UINT64_MAX && statistics.values[18]==UINT64_MAX,"Search counters wrapped");
}
void exact_lifecycle() {
    LoginHub hub;std::string welcome;
    const auto source=join(hub,1,welcome),target=join(hub,2,welcome);(void)join(hub,3,welcome);(void)drain(hub,1);(void)drain(hub,2);
    const auto now=users::TimePoint{};
    hub.receive(1,parse("DSCH "+source+" "+target+" ANStatisticsPrivate TOprivate"),now);
    hub.receive(1,parse("BSCH "+source+" ANStatisticsPrivate"),now);
    hub.receive(1,parse("FSCH "+source+" +ZZZ0 ANStatisticsPrivate"),now);
    hub.receive(1,parse("BSCH "+source+" TOignored"),now);
    hub.receive(1,parse("DSCH "+source+" ZZZZ ANStatisticsPrivate"),now);
    hub.receive(1,parse("BSCH "+source+" ANStatisticsPrivate TOprivate"),now);
    const auto fields=" FN/StatisticsPrivate SI0 TR"+std::string(39,'A');
    hub.receive(2,parse("DRES "+target+" "+source+fields+" TOprivate"),now);
    hub.receive(2,parse("ERES "+target+" "+source+fields),now);
    hub.receive(2,parse("DRES "+target+" ZZZZ"+fields),now);
    hub.receive(2,parse("DRES "+target+" "+source+fields+" TOmissing"),now);
    hub.receive(1,parse("BSCH "+target+" ANforged"),now);
    auto stats=hub.search_statistics();
    require(stats.values==std::array<std::uint64_t,19>{7,3,3,0,1,0,4,1,0,0,4,1,3,0,0,0,1,1,0} && stats.tracked_searches==1,"Search lifecycle totals differ");
    hub.receive(1,parse("BSCH "+source+" ANclosed"),now);
    require(hub.search_statistics()==stats,"Closing session changed totals");
    hub.disconnect(1,false);stats.tracked_searches=0;
    require(hub.search_statistics()==stats && LoginHub{}.search_statistics()==search::Statistics{},"Disconnect changed cumulative counters or restart did not reset");
    require(hub.chat_statistics()==chat::Statistics{},"Search telemetry entered chat totals");
}
void gate_and_error_accounting() {
    LoginLimits limits;limits.search.queries={1,1,32768,131072};limits.search.results={1,1,32768,131072};limits.search.guests.queries=false;
    LoginHub hub(limits);std::string welcome;const auto source=join(hub,1,welcome),target=join(hub,2,welcome),other=join(hub,3,welcome);(void)drain(hub,1);(void)drain(hub,2);
    const auto now=users::TimePoint{};const auto query=parse("DSCH "+source+" "+target+" ANprivate");
    for(int n=0;n<5;++n){hub.receive(1,query,now);}
    require(hub.set_search_permissions(1,{true,true}),"Grant failed");
    hub.receive(1,query,now);hub.receive(1,query,now);
    auto result=[&](const std::string& sender){return parse("DRES "+sender+" "+source+" FNprivate SI0 TR"+std::string(39,'A'));};
    hub.receive(2,result(target),now);hub.receive(3,result(other),now);hub.receive(3,result(other),now);
    require(hub.search_statistics().values==std::array<std::uint64_t,19>{7,1,5,1,0,0,1,0,0,0,3,1,0,1,1,0,1,0,1},"Suppressed denials/recipient limits not counted exactly");
    LoginHub fault;const auto owner=join(fault,1,welcome);const auto candidate=parse("BSCH "+owner+" AN"+std::string(128,'x'));
    fail_allocations=true;fault.receive(1,candidate);const auto failed=fault.search_statistics();fail_allocations=false;
    require(failed.values[0]==1 && failed.values[5]==1 && failed.tracked_searches==0,"Preparation allocation failure lost error outcome");
    LoginLimits small;small.output_per_client=512;LoginHub queues(small);
    const auto a=join(queues,1,welcome);(void)join(queues,2,welcome);(void)drain(queues,1);
    queues.receive(1,parse("BSCH "+a+" ANx XX"+std::string(1000,'x')));
    require(queues.search_statistics().values[1]==1 && queues.search_statistics().values[8]==1 && queues.search_statistics().values[9]==1 && queues.search_statistics().values[6]==0 && queues.search_statistics().values[5]==0,"Queue refusal counted delivery or source failure");
}
}
int main(){try{saturation();exact_lifecycle();gate_and_error_accounting();std::cout<<"PASS: search aggregate telemetry, lifecycle, limits, saturation and privacy\n";}
catch(const std::exception& error){fail_allocations=false;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}}
