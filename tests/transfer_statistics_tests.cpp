#include "dc24h/transfers/statistics.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace { bool deny_allocations=false; int fail_after=-1; }
[[gnu::noinline]] void* operator new(std::size_t n) {
    if(deny_allocations){throw std::bad_alloc();}
    if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0){--fail_after;}
    if(auto* p=std::malloc(n?n:1)){return p;}throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void* p) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void* p) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
namespace {
using namespace dc24h;
using namespace std::chrono_literals;
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
adc::Message parse(std::string_view s){return std::get<adc::Message>(adc::parse_line(s));}
std::string drain(adc::LoginHub& hub,unsigned who){std::string out;while(!hub.output(who).empty()&&!hub.must_close(who)){const std::string part(hub.output(who));out+=part;hub.consume(who,part.size());}return out;}
std::string join(adc::LoginHub& hub,unsigned who,bool active=true){
    hub.connect(who,"127.0.0.1");hub.receive(who,parse("HSUP ADBASE ADTIGR"));hub.consume(who,hub.output(who).size());
    const std::string sid(hub.output(who).substr(5,4));hub.consume(who,hub.output(who).size());
    adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(who);
    hub.receive(who,parse("BINF "+sid+" NIUser"+std::to_string(who)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)+" I40.0.0.0"+(active?" SUTCP4":"")));
    (void)drain(hub,who);return sid;
}
void saturation_and_ownership(){
    transfers::Statistics stats;
    deny_allocations=true;
    {transfers::Attempt attempt(&stats,transfers::Operation::ctm);attempt.outcome(transfers::Outcome::routed);attempt.delivery(2,1,3);attempt.queue_error();}
    {transfers::Attempt attempt(&stats,transfers::Operation::rcm);attempt.outcome(transfers::Outcome::recipient_limited);}
    {transfers::Attempt attempt(nullptr,transfers::Operation::ctm);}
    transfers::peer_report(stats,41);transfers::peer_report(stats,42);transfers::peer_report(stats,99);
    const auto copy=stats;
    stats.values.fill(UINT64_MAX-1);
    for(int i=0;i<2;++i){transfers::Attempt attempt(&stats,transfers::Operation::ctm);attempt.outcome(transfers::Outcome::routed);attempt.delivery(UINT64_MAX,UINT64_MAX,UINT64_MAX);attempt.queue_error();transfers::peer_report(stats,41);}
    deny_allocations=false;
    require(copy.values==std::array<std::uint64_t,22>{1,1,0,0,0,0,2,1,3,1,1,0,0,0,1,0,0,0,0,0,1,1},"Attempt accounting or snapshot ownership failed");
    for(auto i:{0U,1U,6U,7U,8U,9U,20U}){require(stats.values[i]==UINT64_MAX,"Counter wrapped instead of saturating");}
}
void exact_lifecycle(){
    adc::LoginHub hub;const auto a=join(hub,1),b=join(hub,2,false),c=join(hub,3);for(unsigned n=1;n<=3;++n){(void)drain(hub,n);}
    const auto chat=hub.chat_statistics();const auto search=hub.search_statistics();
    hub.receive(1,parse("ECTM "+a+" "+b+" ADCS/1.0 01515 StatisticsPrivate"));
    hub.receive(1,parse("DCTM "+a+" "+b+" ADC/1.0 0 StatisticsPrivate"));
    hub.receive(1,parse("DCTM "+a+" ZZZZ ADC/1.0 1515 StatisticsPrivate"));
    hub.receive(2,parse("DRCM "+b+" "+a+" ADC/1.0 StatisticsPrivate"));
    hub.receive(1,parse("ERCM "+a+" "+b+" ADC/1.0 StatisticsPrivate"));
    hub.receive(2,parse("DSTA "+b+" "+a+" 141 Unsupported PRADCS/1.0 TOStatisticsPrivate"));
    hub.receive(2,parse("ESTA "+b+" "+a+" 242 Failed PRADC/1.0 TOStatisticsPrivate"));
    hub.receive(3,parse("DSTA "+c+" ZZZZ 141 Unavailable PRADC/1.0 TOStatisticsPrivate"));
    require(hub.transfer_statistics().values==std::array<std::uint64_t,22>{3,1,2,0,0,0,2,0,0,0,2,1,1,0,0,0,1,0,0,0,1,1},"Logical attempts, E copies or peer reports were miscounted");
    const auto before=hub.transfer_statistics();for(unsigned n=1;n<=3;++n){(void)drain(hub,n);}
    require(hub.transfer_statistics()==before,"Output completion counted another attempt");
    hub.receive(1,parse("DCTM "+b+" "+c+" ADC/1.0 1515 ForgedPrivate"));
    auto expected=before;expected.values[0]++;expected.values[2]++;
    require(hub.transfer_statistics()==expected,"Early forged source was not rejected exactly once");
    hub.receive(1,parse("DCTM "+a+" "+c+" ADC/1.0 1515 ClosingPrivate"));
    require(hub.transfer_statistics()==expected,"Closing source changed cumulative totals");
    for(unsigned n=1;n<=3;++n){hub.disconnect(n,false);}
    require(hub.transfer_statistics()==expected&&hub.queued_bytes()==0,"Disconnect reset totals or leaked queues");
    require(hub.chat_statistics()==chat&&hub.search_statistics()==search,"Transfer metrics entered Chat/Search");
    require(adc::LoginHub{}.transfer_statistics()==transfers::Statistics{},"Fresh hub retained old counters");
}
void limits_permissions_and_errors(){
    for(bool reverse:{false,true}){
        adc::LoginLimits limits;limits.transfers.flood.requests={1,1,131072,262144};limits.transfers.flood.incoming={1,1,131072,262144};
        adc::LoginHub hub(limits);const auto a=join(hub,1,!reverse),b=join(hub,2),c=join(hub,3,!reverse);for(unsigned n=1;n<=3;++n){(void)drain(hub,n);}
        const auto command=std::string(reverse?"RCM":"CTM");const auto fields=std::string(reverse?" ADC/1.0 Token":" ADC/1.0 1515 Token");
        const auto request=parse("D"+command+" "+a+" "+b+fields),other=parse("E"+command+" "+c+" "+b+fields);
        const users::TimePoint now{};
        hub.receive(1,request,now);hub.receive(1,request,now);hub.receive(1,request,now);hub.receive(3,other,now);
        auto stats=hub.transfer_statistics();const auto base=reverse?10U:0U;
        require(stats.values[base]==4&&stats.values[base+1]==1&&stats.values[base+3]==2&&stats.values[base+4]==1&&stats.values[base+6]==1,"Silent/source/target quota denial accounting differs");
        require(hub.set_transfer_permissions(1,{false,true}),"Trusted grant update failed");hub.receive(1,request,now+1s);
        stats=hub.transfer_statistics();require(stats.values[base]==5&&stats.values[base+2]==1,"Permission denial was not counted");
        (void)hub.set_transfer_permissions(1,{});const auto message=parse("D"+command+" "+a+" "+b+fields);
        deny_allocations=true;hub.receive(1,message,now+2s);const auto failure=hub.transfer_statistics();deny_allocations=false;
        require(failure.values[base]==6&&failure.values[base+5]==1,"Allocation failure escaped the attempt or snapshot allocated");
    }
}
void allocation_sweep(){
    bool saw_error=false,saw_queue_error=false,saw_success=false;
    for(int failure=0;failure<128;++failure){
        adc::LoginHub hub;const auto a=join(hub,1),b=join(hub,2);(void)join(hub,3);for(unsigned n=1;n<=3;++n){(void)drain(hub,n);}
        const auto request=parse("ECTM "+a+" "+b+" ADC/1.0 1515 StatisticsAllocationPrivate");
        fail_after=failure;hub.receive(1,request);fail_after=-1;
        const auto stats=hub.transfer_statistics();const auto& v=stats.values;
        require(v[0]==1&&v[1]+v[2]+v[3]+v[4]+v[5]==1,"Fault path lost or duplicated its terminal outcome");
        require(v[9]<=v[7]&&v[6]+v[7]+v[8]<=2,"Fault path delivery totals invalid");
        require(hub.ready(3),"Telemetry fault closed an unrelated user");
        saw_error=saw_error||v[5]>0;saw_queue_error=saw_queue_error||v[9]>0;saw_success=saw_success||v[6]==2;
    }
    require(saw_error&&saw_queue_error&&saw_success,"Allocation sweep did not reach source/recipient/success paths");
}
}
int main(){try{saturation_and_ownership();exact_lifecycle();limits_permissions_and_errors();allocation_sweep();std::cout<<"Transfer statistics passed\n";}catch(const std::exception& error){deny_allocations=false;fail_after=-1;std::cerr<<error.what()<<'\n';return 1;}}
