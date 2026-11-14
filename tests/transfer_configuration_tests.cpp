#include "dc24h/transfers/protocol_policy.hpp"
#include "dc24h/adc/connection_routing.hpp"
#include "dc24h/adc/connection_errors.hpp"
#include "dc24h/configuration/configuration.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <unistd.h>

namespace { bool deny_allocations=false; }
[[gnu::noinline]] void* operator new(std::size_t n) { if(deny_allocations){throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1)){return p;}throw std::bad_alloc(); }
[[gnu::noinline]] void* operator new[](std::size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
namespace {
using namespace dc24h;
using namespace dc24h::transfers;
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
template<class F> void invalid(F f){bool refused=false;try{f();}catch(const std::invalid_argument&){refused=true;}require(refused,"Invalid policy/configuration accepted");}
void parsing() {
    require(ProtocolPolicy{}.any()&&ProtocolPolicy::parse("any").allows("future/1.0"),"Default lost extension compatibility");
    require(!ProtocolPolicy::parse(" none ").allows("ADC/1.0"),"none permitted a protocol");
    std::string input="\tADC/1.0 , ADCS/1.0,Future\"\\Protocol/1.0 ";
    const auto policy=ProtocolPolicy::parse(input);input.assign("mutated");
    auto copy=policy;
    deny_allocations=true;
    const bool allowed=copy.allows("ADC/1.0")&&copy.allows("ADCS/1.0")&&copy.allows("Future\"\\Protocol/1.0");
    const bool refused=!copy.allows("adc/1.0")&&!copy.allows("ADCS/1.1")&&!copy.allows("Future/1.0");
    deny_allocations=false;
    require(allowed&&refused&&!policy.any()&&policy.names().size()==3,"Exact owned allocation-free policy differs");
    require(!ProtocolPolicy::parse("*").allows("ADC/1.0"),"Wildcard was inferred");
    require(ProtocolPolicy::parse("adc/1.0").allows("adc/1.0"),"Case was normalized");
    for(const auto bad:{"", " ", ",ADC/1.0", "ADC/1.0,", "ADC/1.0,,ADCS/1.0", "ADC/1.0,ADC/1.0", "any,ADC/1.0", "ADC/1.0,none", "ADC /1.0", "ADC\t/1.0", "żółw/1.0", "ADC\n/1.0"}) {
        invalid([&]{(void)ProtocolPolicy::parse(bad);});
    }
    require(ProtocolPolicy::parse(std::string(32,'x')).names().front().size()==32,"32-byte name refused");
    invalid([&]{(void)ProtocolPolicy::parse(std::string(33,'x'));});
    std::string names;
    for(unsigned n=0;n<16;++n){if(n){names+=',';}names+="extension/"+std::to_string(n);}
    require(ProtocolPolicy::parse(names).names().size()==16,"16 names refused");
    invalid([&]{(void)ProtocolPolicy::parse(names+",extension/16");});
}
users::SessionId id(unsigned n){return *users::SessionId::from_value(n);}
void add(users::SessionManager& sessions,unsigned n,bool active) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n),users::Transport::adc,{})),"Add failed");
    require(!sessions.start_negotiation(id(n),{}),"Start failed");
    users::ProfileData p;p.ipv4="127.0.0.1";p.features=active?std::vector<std::string>{"TCP4","ADCS"}:std::vector<std::string>{"ADCS"};
    require(!sessions.identify(id(n),*users::User::create(*users::Identity::create(n==1?"AAAA":"BBBB","CID"+std::to_string(n),"User"+std::to_string(n)),*users::Profile::create(p)),{}),"Identity failed");
    require(!sessions.mark_ready(id(n),{}),"Ready failed");
}
void routing() {
    users::SessionManager sessions(4);add(sessions,1,true);add(sessions,2,false);
    for(const auto allowed:{"any","none","ADC/1.0","ADCS/1.0","future/1.0","ADC/1.0,ADCS/1.0"}) {
        SecuritySettings settings;settings.allowed_protocols=ProtocolPolicy::parse(allowed);
        const auto expected=settings.allowed_protocols;
        Core core(sessions,settings);adc::ConnectionRouter router(core);
        settings.allowed_protocols=ProtocolPolicy::parse("none"); // Core owns its startup copy.
        for(bool reverse:{false,true})for(bool echo:{false,true})for(const auto protocol:{"ADC/1.0","ADCS/1.0","future/1.0"}) {
            const auto wire=std::string(echo?"E":"D")+(reverse?"RCM BBBB AAAA ":"CTM AAAA BBBB ")+protocol+(reverse?" Token":" 01515 Token");
            const auto message=std::get<adc::Message>(adc::parse_line(wire));
            auto prepared=router.prepare(id(reverse?2U:1U),message);
            if(!expected.allows(protocol)) {
                require(std::get<adc::ConnectionRoutingError>(prepared)==adc::ConnectionRoutingError::protocol_forbidden,"Disallowed protocol passed routing");
                continue;
            }
            const auto route=std::get<adc::RoutedConnection>(std::move(prepared));
            unsigned copies=0;
            const auto expected_wire=wire+'\n';
            const adc::ConnectionRouter::Sink sink=[&](auto,const auto& frame){++copies;return frame->wire()==expected_wire;};
            deny_allocations=true;const auto result=router.dispatch(route,sink);deny_allocations=false;
            require(std::get<adc::ConnectionDispatchReport>(result).queued==(echo?2U:1U)&&copies==(echo?2U:1U),"Allowed request changed its protocol or audience");
        }
    }
    for(const auto command:{adc::Command::ctm,adc::Command::rcm}) {
        const auto diagnostic=adc::connection_failure(adc::ConnectionRoutingError::protocol_forbidden,command);
        require(diagnostic.action==adc::FailureAction::reply&&diagnostic.code=="125","Policy denial became a peer protocol report or disconnect");
    }
}
void ini() {
    char pattern[]="/tmp/dc24h-transfer-conf-XXXXXX";const auto* directory=mkdtemp(pattern);require(directory,"mkdtemp failed");
    const std::filesystem::path root(directory);
    struct Cleanup{std::filesystem::path root;~Cleanup(){std::error_code ec;std::filesystem::remove_all(root,ec);}} cleanup{root};
    const auto path=root/"hub.ini";
    const auto load=[&](std::string_view content){std::ofstream out(path);out<<content;out.close();auto settings=configuration::load_file(path.string());configuration::validate(settings);return settings;};
    const auto settings=load("[transfers]\nallowed_protocols = ADCS/1.0\nrequest_rate=1\nrequest_burst=2\nincoming_rate=3\nincoming_burst=4\n");
    require(settings.network.transfers.allowed_protocols.allows("ADCS/1.0")&&!settings.network.transfers.allowed_protocols.allows("ADC/1.0"),"INI policy lost");
    require(settings.network.transfers.flood.requests.rate==1&&settings.network.transfers.flood.incoming.burst==4,"INI limits lost");
    for(const auto bad:{"allowed_protocols=\n","allowed_protocols=ADC/1.0,ADC/1.0\n","allowed_protocols=any\nallowed_protocols=none\n","allowed_protocols_extra=any\n","allowed_protocols=any,none\n","request_byte_burst=16383\n"}) {
        invalid([&]{(void)load(std::string("[transfers]\n")+bad);});
    }
}
}
int main(){try{parsing();routing();ini();std::cout<<"Transfer configuration checks passed\n";}catch(const std::exception& e){deny_allocations=false;std::cerr<<e.what()<<'\n';return 1;}}
