#include "dc24h/transfers/limits.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace { bool deny_allocations=false; }
[[gnu::noinline]] void* operator new(std::size_t n) { if(deny_allocations){throw std::bad_alloc();} if(auto* p=std::malloc(n?n:1)){return p;} throw std::bad_alloc(); }
[[gnu::noinline]] void* operator new[](std::size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
namespace {
using namespace dc24h;
using namespace dc24h::transfers;
using namespace std::chrono_literals;
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
adc::Message parse(std::string_view s){return std::get<adc::Message>(adc::parse_line(s));}
void guard_boundaries() {
    const users::TimePoint start{};
    Limits limits;limits.requests={2,2,16384,16384};limits.incoming={1,1,16384,16384};
    TransferGuard guard(limits);
    deny_allocations=true;
    const bool first=guard.admit(Direction::outgoing,8192,start);
    const bool bytes_denied=!guard.admit(Direction::outgoing,8193,start);
    const bool atomic=guard.admit(Direction::outgoing,8192,start);
    const bool count_denied=!guard.admit(Direction::outgoing,0,start);
    const bool incoming=guard.admit(Direction::incoming,16384,start);
    const bool notice=guard.notice(start)&&!guard.notice(start);
    const bool fractional=!guard.admit(Direction::outgoing,8192,start+499ms)&&guard.admit(Direction::outgoing,8192,start+500ms);
    const bool backwards=!guard.admit(Direction::outgoing,1,start-1s);
    const bool second_notice=!guard.notice(start+999ms)&&guard.notice(start+1s)&&!guard.notice(start+1s);
    const bool huge=!guard.admit(Direction::incoming,std::numeric_limits<std::size_t>::max(),start+1s);
    const bool no_partial_debit=guard.admit(Direction::incoming,16384,start+1s);
    const bool invalid=!guard.admit(static_cast<Direction>(99),0,users::TimePoint::max());
    const bool saturated=guard.admit(Direction::outgoing,16384,users::TimePoint::max());
    const bool no_rewind=!guard.admit(Direction::outgoing,1,users::TimePoint::min());
    deny_allocations=false;
    require(first&&bytes_denied&&atomic&&count_denied&&incoming,"Count/byte atomicity or direction isolation failed");
    require(notice&&fractional&&backwards&&second_notice,"Refill/notice monotonicity failed");
    require(huge&&no_partial_debit&&invalid&&saturated&&no_rewind,"Overflow, invalid direction or extreme clock failed");
    TransferGuard extreme(limits);
    require(extreme.admit(Direction::outgoing,16384,users::TimePoint::min())&&extreme.admit(Direction::outgoing,16384,users::TimePoint::max()),"Extreme clock subtraction overflowed");
    for(bool incoming_limits:{false,true})for(unsigned member=0;member<4;++member)for(auto bad:{0U,16777217U}) {
        auto broken=limits;auto& bucket=incoming_limits?broken.incoming:broken.requests;
        if(member==0){bucket.rate=bad;}else if(member==1){bucket.burst=bad;}else if(member==2){bucket.byte_rate=bad;}else{bucket.byte_burst=bad;}
        bool rejected=false;try{TransferGuard ignored(broken);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Invalid native limit accepted");
    }
    limits.requests.byte_burst=16383;bool rejected=false;try{TransferGuard ignored(limits);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Sub-frame byte burst accepted");
}
std::string drain(adc::LoginHub& hub,unsigned who) {
    std::string out;while(!hub.output(who).empty()&&!hub.must_close(who)){const std::string part(hub.output(who));out+=part;hub.consume(who,part.size());}return out;
}
std::string join(adc::LoginHub& hub,unsigned who,bool active=true) {
    hub.connect(who,"127.0.0.1");hub.receive(who,parse("HSUP ADBASE ADTIGR"));hub.consume(who,hub.output(who).size());
    const std::string sid(hub.output(who).substr(5,4));hub.consume(who,hub.output(who).size());
    adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(who);
    hub.receive(who,parse("BINF "+sid+" NIUser"+std::to_string(who)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)+" I40.0.0.0"+(active?" SUTCP4":"")));
    (void)drain(hub,who);return sid;
}
void source_and_command_sharing() {
    adc::LoginLimits limits;limits.transfers.flood.requests={1,2,131072,262144};adc::LoginHub hub(limits);
    const auto a=join(hub,1),b=join(hub,2),c=join(hub,3);for(unsigned n=1;n<=3;++n){(void)drain(hub,n);}
    const users::TimePoint now{};
    auto wire="ECTM "+a+" "+b+" ADC/1.0 01515 PrivateLimitToken";
    hub.receive(1,parse(wire),now);require(drain(hub,1)==wire+"\n"&&drain(hub,2)==wire+"\n","Initial E request failed");
    hub.receive(1,parse("BINF "+a+" SU"));for(unsigned n=1;n<=3;++n){(void)drain(hub,n);}
    wire="DRCM "+a+" "+c+" ADCS/1.0 PrivateLimitToken";
    hub.receive(1,parse(wire),now);require(drain(hub,3)==wire+"\n","RCM did not share source budget");
    hub.receive(1,parse("BINF "+a+" SUTCP4"));for(unsigned n=1;n<=3;++n){(void)drain(hub,n);}
    require(hub.set_transfer_permissions(1,{false,false})&&hub.set_transfer_permissions(1,{}),"Permission update failed");
    wire="ECTM "+a+" "+b+" ADCS/1.0 1515 PrivateLimitToken";
    hub.receive(1,parse(wire),now);
    require(drain(hub,1)=="ISTA 100 Connection\\srequest\\srate\\slimit\\sexceeded FCECTM\n"&&hub.output(2).empty()&&hub.output(3).empty(),"Flood leaked target or echo");
    for(unsigned n=0;n<100;++n){hub.receive(1,parse(wire),now);}
    require(hub.output(1).empty(),"Denial notifications amplified flood");
    hub.receive(1,parse(wire),now+1s);require(drain(hub,1)==wire+"\n"&&drain(hub,2)==wire+"\n","Refill did not restore routing");
    // Protocol/source failures remain terminal while rate credit is empty.
    hub.receive(1,parse("DCTM "+b+" "+c+" ADC/1.0 1515 Forgery"),now+1s);
    require(hub.closing(1)||hub.must_close(1),"Exhaustion hid source forgery");
    require(hub.ready(2)&&hub.ready(3),"Source rejection affected bystanders");
}
void aggregate_target_and_cleanup() {
    adc::LoginLimits limits;limits.transfers.flood.incoming={1,1,131072,262144};adc::LoginHub hub(limits);
    const auto a=join(hub,1),b=join(hub,2),c=join(hub,3);for(unsigned n=1;n<=3;++n){(void)drain(hub,n);}
    const users::TimePoint now{};
    const auto first="DCTM "+a+" "+b+" ADC/1.0 1515 SharedOpaqueToken";
    hub.receive(1,parse(first),now);require(drain(hub,2)==first+"\n","First target request failed");
    const auto second="ECTM "+c+" "+b+" ADCS/1.0 1515 SharedOpaqueToken";
    hub.receive(3,parse(second),now);
    require(drain(hub,3).starts_with("ISTA 100 ")&&hub.output(1).empty()&&hub.output(2).empty(),"Target aggregate limit bypassed");
    // Receiving traffic does not consume outgoing allowance.
    const auto reverse="DCTM "+b+" "+a+" ADC/1.0 1515 SharedOpaqueToken";
    hub.receive(2,parse(reverse),now);require(drain(hub,1)==reverse+"\n","Incoming traffic spent source allowance");
    hub.disconnect(2);for(unsigned n:{1U,3U}){(void)drain(hub,n);}
    const auto fresh=join(hub,4);for(unsigned n:{1U,3U}){(void)drain(hub,n);}
    const auto replacement="DCTM "+c+" "+fresh+" ADC/1.0 1515 SharedOpaqueToken";
    hub.receive(3,parse(replacement),now);require(drain(hub,4)==replacement+"\n","New lifetime inherited target exhaustion");
    for(unsigned n:{1U,3U,4U}){hub.disconnect(n,false);}
    require(hub.queued_bytes()==0&&hub.users()==0,"Guard lifetime cleanup leaked queues/users");
}
}
int main(){try{guard_boundaries();source_and_command_sharing();aggregate_target_and_cleanup();std::cout<<"Transfer limits passed\n";}catch(const std::exception& error){deny_allocations=false;std::cerr<<error.what()<<'\n';return 1;}}
