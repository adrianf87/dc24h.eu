#include "dc24h/search/security.hpp"
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

using search::Operation;
void buckets_and_bounds() {
    const auto start=users::TimePoint{};
    search::SecuritySettings settings;settings.queries={5,2,32768,131072};settings.results={1,1,1,16384};
    search::SearchGuard guard(settings);
    require(guard.admit(Operation::query,10,start) && guard.admit(Operation::query,10,start) && !guard.admit(Operation::query,10,start),"Query burst differs");
    require(guard.notice(start) && !guard.notice(start) && !guard.admit(Operation::query,10,start+200ms-1ns) && guard.admit(Operation::query,10,start+200ms),"Notice/refill precision differs");
    require(guard.admit(Operation::result,16384,start+200ms) && !guard.admit(Operation::result,1,start+200ms),"Result debit differs");
    require(guard.admit(Operation::incoming_result,16384,start+200ms) && !guard.admit(Operation::incoming_result,1,start+200ms),"Incoming result budget missing/aliased");
    require(!guard.notice(start+1s-1ns) && guard.notice(start+1s),"Operation switching reset notice window");
    settings.queries={1,2,1,16384};search::SearchGuard bytes(settings);
    require(bytes.admit(Operation::query,16000,start) && !bytes.admit(Operation::query,385,start) && bytes.admit(Operation::query,384,start),"Byte denial partially debited count");
    settings.queries={1,1,1,16384};search::SearchGuard count(settings);
    require(count.admit(Operation::query,10000,start) && !count.admit(Operation::query,5000,start) && count.admit(Operation::query,6385,start+1s),"Count denial partially debited bytes");
    search::SearchGuard extremes(settings);
    require(extremes.admit(Operation::query,0,users::TimePoint::min()) && !extremes.admit(Operation::query,std::numeric_limits<std::size_t>::max(),users::TimePoint::min()) &&
        extremes.admit(Operation::query,16384,users::TimePoint::max()) && !extremes.admit(Operation::query,1,users::TimePoint::min()),"Extreme clock/size arithmetic failed");
    search::SearchGuard bounded;std::size_t accepted=0,notices=0;
    deny_allocations=true;
    for (unsigned n=0;n<10000;++n) { accepted+=bounded.admit(Operation::query,1,start);notices+=bounded.notice(start); }
    deny_allocations=false;
    require(accepted==32 && notices==1,"Guard allocates/grows or amplifies denials");
    for (int member=0;member<4;++member) {
        for (const auto value:{0U,std::numeric_limits<std::uint32_t>::max()}) {
            auto bad=settings;
            switch (member) {case 0:bad.queries.rate=value;break;case 1:bad.queries.burst=value;break;case 2:bad.results.byte_rate=value;break;default:bad.results.byte_burst=value;break;}
            bool rejected=false;try {search::validate_security(bad);}catch(const std::invalid_argument&){rejected=true;}
            require(rejected,"Invalid security quota accepted");
        }
    }
    auto bad=settings;bad.tokens.total=1;bad.tokens.per_owner=2;
    bool rejected=false;try {search::validate_security(bad);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Invalid token cross-limit accepted");
    require(!search::authorize(settings,{true,true},Operation::query) && search::authorize(settings,{false,true},Operation::query)==search::PermissionError::forbidden,"Query permission not checked");
    settings.results_enabled=false;
    require(search::authorize(settings,{true,true},Operation::result)==search::PermissionError::disabled,"Grant bypassed global disable");
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

void live_permissions_and_limits() {
    adc::LoginLimits limits;limits.search.queries={1,1,32768,131072};limits.search.results={1,1,32768,131072};
    adc::LoginHub hub(limits);const auto source=join(hub,1), target=join(hub,2), other=join(hub,3);(void)drain(hub,1);(void)drain(hub,2);
    const auto now=users::TimePoint{};
    const auto query=adc::Message(std::get<adc::Message>(adc::parse_line("DSCH "+source+" "+target+" ANSecurityPrivate TOPrivateToken")));
    hub.receive(1,query,now);require(drain(hub,2).starts_with("DSCH "+source) && hub.output(1).empty() && hub.output(3).empty(),"Allowed query not routed privately");
    const auto denied=parse("DSCH "+source+" "+target+" ANSecurityPrivate TORefusedToken");
    hub.receive(1,denied,now);
    require(drain(hub,1)=="ISTA 100 Search\\srate\\slimit\\sexceeded FCDSCH\n" && hub.output(2).empty(),"Query flood reached recipient");
    for (int n=0;n<100;++n) {hub.receive(1,denied,now);}
    require(hub.output(1).empty() && hub.output(2).empty(),"Repeated denials amplified output");
    // Rejected requests did not reserve TO; the same value routes after refill.
    hub.receive(1,denied,now+1s);require(drain(hub,2).find("TORefusedToken")!=std::string::npos,"Flood denial reserved a token");
    require(hub.set_search_permissions(1,{false,true}),"Trusted revocation failed");
    const auto result=parse("DRES "+target+" "+source+" FN/SecurityPrivate SI0 TR"+std::string(39,'A')+" TOPrivateToken");
    hub.receive(2,result,now+1s);require(drain(hub,2)=="ISTA 100 Search\\sresult\\stoken\\sis\\snot\\sactive FCDRES\n" && hub.output(1).empty(),"Revocation retained active query window");
    hub.receive(1,query,now+2s);require(drain(hub,1)=="ISTA 125 Search\\soperation\\snot\\spermitted FCDSCH\n","Permission denial not applied");
    // A public support update is never a capability grant.
    hub.receive(1,parse("HSUP ADTCP4"),now+2s);hub.receive(1,query,now+2s);
    require(hub.output(1).empty() && hub.output(2).empty(),"SUP bypassed permission/notice throttle");
    require(hub.set_search_permissions(1,{true,true}),"Trusted grant failed");
    hub.receive(1,query,now+3s);require(drain(hub,2).starts_with("DSCH "+source),"Grant did not permit a new query");
    // Incoming quota protects one recipient across independent senders.
    auto tokenless=[&](const std::string& sender){return parse("DRES "+sender+" "+source+" FN/SecurityPrivate SI0 TR"+std::string(39,'A'));};
    hub.receive(2,tokenless(target),now+3s);require(drain(hub,1).starts_with("DRES "+target),"First result lost");
    hub.receive(3,tokenless(other),now+3s);
    require(hub.output(1).empty() && drain(hub,3)=="ISTA 100 Search\\srecipient\\srate\\slimit\\sexceeded FCDRES\n" && hub.ready(1) && hub.ready(3),"Multi-sender recipient flood not isolated");
    // Permission changes preserve outgoing rate credits.
    require(hub.set_search_permissions(3,{true,false}) && hub.set_search_permissions(3,{true,true}),"Permission update failed");
    hub.receive(3,tokenless(other),now+3s);require(hub.output(1).empty() && hub.output(3).empty(),"Permission update minted rate credit/notice");
    hub.receive(3,tokenless(other),now+4s);require(drain(hub,1).starts_with("DRES "+other),"Result refill failed");
    require(hub.chat_statistics()==chat::Statistics{},"Search security counted as chat");
    require(!hub.set_search_permissions(99,{true,true}),"Missing lifetime accepted permissions");
    hub.disconnect(1,false);require(!hub.set_search_permissions(1,{true,true}),"Removed lifetime accepted permissions");
}
void decoded_budget_and_malformed_attempts() {
    adc::LoginLimits limits;limits.search.queries={1000,100,1,16384};
    adc::LoginHub hub(limits);const auto source=join(hub,1), target=join(hub,2);(void)drain(hub,1);
    const auto now=users::TimePoint{};
    const auto frame=parse("DSCH "+source+" "+target+" ANx XX"+std::string(4080,'x'));
    for(int n=0;n<3;++n){hub.receive(1,frame,now);require(drain(hub,2).starts_with("DSCH "+source),"Byte fixture did not route");}
    hub.receive(1,frame,now);
    require(drain(hub,1)=="ISTA 100 Search\\srate\\slimit\\sexceeded FCDSCH\n" && hub.output(2).empty(),"Unknown-field/header byte costs were omitted");
    auto huge=frame;huge.parameters={"ANx","XX"+std::string(1024*1024,'x')};
    deny_allocations=true;hub.receive(1,huge,now);deny_allocations=false;
    require(hub.ready(1) && hub.output(1).empty() && hub.output(2).empty(),"Huge input bypassed byte admission/notice cap or allocated");
    hub.receive(1,parse("DSCH "+source+" "+target+" ANsmall"),now);
    require(drain(hub,2).find("ANsmall")!=std::string::npos,"Byte denial consumed residual credit");
    adc::LoginLimits one;one.search.queries={1,1,32768,131072};adc::LoginHub malformed(one);
    const auto a=join(malformed,1),b=join(malformed,2);(void)drain(malformed,1);
    malformed.receive(1,parse("DSCH "+a+" "+b+" AN"),now);
    require(drain(malformed,1)=="ISTA 100 Invalid\\ssearch\\squery FCDSCH\n","Malformed semantic fixture changed");
    malformed.receive(1,parse("DSCH "+a+" "+b+" ANx"),now);
    require(drain(malformed,1)=="ISTA 100 Search\\srate\\slimit\\sexceeded FCDSCH\n" && malformed.output(2).empty(),"Malformed attempts escaped quota");
}
void defaults_and_strict_token() {
    adc::LoginLimits limits;limits.search.guests={false,false};limits.search.require_result_token=true;
    adc::LoginHub hub(limits);const auto source=join(hub,1), target=join(hub,2);(void)drain(hub,1);
    const auto now=users::TimePoint{};
    const auto query=parse("DSCH "+source+" "+target+" ANx TOprivate");
    hub.receive(1,query,now);require(drain(hub,1).starts_with("ISTA 125 ") && hub.output(2).empty(),"Guest permission ignored");
    require(hub.set_search_permissions(1,{true,false}) && hub.set_search_permissions(2,{false,true}),"Trusted split permissions failed");
    hub.receive(1,query,now+1s);(void)drain(hub,2);
    const auto tokenless=parse("DRES "+target+" "+source+" FNfile SI0 TR"+std::string(39,'A'));
    hub.receive(2,tokenless,now+1s);require(drain(hub,2)=="ISTA 100 Search\\sresult\\stoken\\srequired FCDRES\n" && hub.output(1).empty(),"Strict token policy bypassed");
    hub.receive(2,parse("DRES "+target+" "+source+" FNfile SI0 TR"+std::string(39,'A')+" TOprivate"),now+2s);
    require(drain(hub,1).starts_with("DRES "+target),"Strict valid token refused");
    hub.disconnect(1,false);(void)drain(hub,2);const auto again=join(hub,3);(void)drain(hub,2);
    hub.receive(3,parse("DSCH "+again+" "+target+" ANx"),now+3s);
    require(drain(hub,3).starts_with("ISTA 125 ") && hub.output(2).empty(),"Reconnect inherited trusted grant");
    hub.receive(3,parse("DSCH "+target+" "+again+" ANx"),now+3s);
    require(drain(hub,3).starts_with("ISTA 240 "),"Denied policy masked wrong-source fatal error");
}
}
int main() {
    try {buckets_and_bounds();live_permissions_and_limits();decoded_budget_and_malformed_attempts();defaults_and_strict_token();
        std::cout<<"PASS: search rate/byte/recipient limits, permissions, token policy and denial bounds\n";
    } catch(const std::exception& error){deny_allocations=false;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
