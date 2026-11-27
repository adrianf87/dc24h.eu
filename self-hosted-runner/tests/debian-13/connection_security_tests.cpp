#include "dc24h/transfers/security.hpp"
#include "dc24h/adc/connection_routing.hpp"
#include "dc24h/adc/ctm.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include <arpa/inet.h>
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
void require(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
users::SessionId id(unsigned n){return *users::SessionId::from_value(n);}
adc::Message parse(std::string_view s){return std::get<adc::Message>(adc::parse_line(s));}
struct Grants final : PermissionSource {
    std::array<std::optional<Permissions>,4> values;
    Grants(){values.fill(Permissions{});}
    std::optional<Permissions> connection_permissions(users::SessionId owner) const noexcept override {
        return owner.value()<values.size()?values[owner.value()]:std::nullopt;
    }
};
void add(users::SessionManager& sessions,unsigned n) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n),users::Transport::adc,{})),"Add failed");
    require(!sessions.start_negotiation(id(n),{}),"Start failed");
    users::ProfileData p;p.ipv4="127.0.0.1";if(n==1){p.features={"TCP4"};}
    require(!sessions.identify(id(n),*users::User::create(*users::Identity::create(n==1?"AAAA":"BBBB","CID"+std::to_string(n),"User"+std::to_string(n)),*users::Profile::create(p)),{}),"Identify failed");
    require(!sessions.mark_ready(id(n),{}),"Ready failed");
}
adc::Message request(bool reverse=false,bool echo=false) {
    return parse(std::string(echo?"E":"D")+(reverse?"RCM BBBB AAAA":"CTM AAAA BBBB")+" ADCS/1.0 "+(reverse?"":"01515 ")+"SecurityPrivate");
}
void addresses_ports_and_sids() {
    for(auto family:{users::AddressFamily::ipv4,users::AddressFamily::ipv6}) {
        for(const auto text: family==users::AddressFamily::ipv4?std::vector<const char*>{"127.0.0.1","10.0.0.1","192.168.1.2"}:std::vector<const char*>{"::1","fd00::1","2001:db8::1"}) {
            users::AddressBytes bytes{};require(inet_pton(family==users::AddressFamily::ipv4?AF_INET:AF_INET6,text,bytes.data())==1,"Fixture address invalid");
            deny_allocations=true;const bool usable=users::usable_address(bytes,family);deny_allocations=false;require(usable,"LAN/loopback address refused");
        }
    }
    for(const auto text:{"::","ff02::1","fe80::1","::ffff:127.0.0.1"}) {
        users::AddressBytes bytes{};(void)inet_pton(AF_INET6,text,bytes.data());require(!users::usable_address(bytes,users::AddressFamily::ipv6),"Unsafe IPv6 listener accepted");
    }
    for(const auto text:{"0.0.0.0","224.0.0.1","255.255.255.255"}) {
        users::AddressBytes bytes{};(void)inet_pton(AF_INET,text,bytes.data());require(!users::usable_address(bytes,users::AddressFamily::ipv4),"Unsafe IPv4 listener accepted");
    }
    ClientCapabilities malformed; malformed.tcp4=users::AddressBytes{127,0,0,1};malformed.tcp4->back()=1;
    require(!malformed.listener(AddressFamily::ipv4)&&!select_connection(malformed,{}),"Noncanonical padding bypassed snapshot validation");
    require(!users::usable_address({},static_cast<AddressFamily>(99)),"Unknown family accepted");
    for(const auto port:std::array<std::int64_t,5>{std::numeric_limits<std::int64_t>::min(),-1,0,65536,std::numeric_limits<std::int64_t>::max()}) {
        require(!valid_connection_port(port),"Invalid signed port admitted");
    }
    require(valid_connection_port(1)&&valid_connection_port(65535),"Port boundary refused");
    for(const auto sid:{"","AAA","AAAAA","aaaa","AAA0","AAA1","AAA8","AAA9"}) {
        auto message=request();message.target=sid;
        require(std::holds_alternative<adc::CtmError>(adc::validate_ctm(message)),"Invalid target SID bypassed wire validation");
    }
}
void authorization_matrix() {
    SecuritySettings settings;
    for(unsigned bits=0;bits<16;++bits)for(auto kind:{Kind::connect_to_me,Kind::reverse_connect}) {
        const Permissions sender{bool(bits&1),bool(bits&2)},target{bool(bits&4),bool(bits&8)};
        deny_allocations=true;const auto result=authorize(settings,sender,target,kind);deny_allocations=false;
        require(result.has_value()==(!(bits&1)||!(bits&8)),"Permission direction matrix differs");
        settings.ctm_enabled=settings.rcm_enabled=false;
        require(authorize(settings,sender,target,kind)==PermissionError::disabled,"Grant bypassed global disable");
        settings={};
    }
    require(authorize(settings,{}, {},static_cast<Kind>(99))==PermissionError::forbidden,"Unknown operation authorized");
}
void revalidation_and_echo() {
    users::SessionManager sessions(4);add(sessions,1);add(sessions,2);Grants grants;Core core(sessions,{},&grants);adc::ConnectionRouter router(core);
    for(bool reverse:{false,true})for(bool echo:{false,true})for(bool sender_denial:{false,true}) {
        grants.values.fill(Permissions{});const auto message=request(reverse,echo);const auto sender=reverse?2U:1U,target=reverse?1U:2U;
        auto route=std::get<adc::RoutedConnection>(router.prepare(id(sender),message));auto copy=route;
        if(sender_denial){grants.values[sender]->initiate=false;}else{grants.values[target]->receive=false;}
        unsigned calls=0;const adc::ConnectionRouter::Sink sink=[&](auto,const auto&){++calls;return true;};
        deny_allocations=true;const auto failed=router.dispatch(route,sink);deny_allocations=false;
        require(std::get<adc::ConnectionRoutingError>(failed)==adc::ConnectionRoutingError::forbidden&&!calls,"Revoked plan reached sink");
        grants.values.fill(Permissions{});
        require(std::get<adc::ConnectionRoutingError>(router.dispatch(copy,sink))==adc::ConnectionRoutingError::already_dispatched,"Permission retry replayed plan");
        auto fresh=std::get<adc::RoutedConnection>(router.prepare(id(sender),message));
        require(std::get<adc::ConnectionDispatchReport>(router.dispatch(fresh,sink)).queued==(echo?2U:1U),"New granted request failed");
    }
    for(bool reverse:{false,true}) {
        grants.values.fill(Permissions{});const auto sender=reverse?2U:1U,target=reverse?1U:2U;
        auto route=std::get<adc::RoutedConnection>(router.prepare(id(sender),request(reverse,true)));
        unsigned calls=0;
        const auto report=router.dispatch(route,[&](auto recipient,const auto&){require(recipient==id(target),"Revoked echo reached source");++calls;grants.values[sender]->initiate=false;return true;});
        require(std::get<adc::ConnectionDispatchReport>(report)==adc::ConnectionDispatchReport{1,0,1}&&calls==1,"Echo ignored source revocation");
        grants.values.fill(Permissions{});
        route=std::get<adc::RoutedConnection>(router.prepare(id(sender),request(reverse,true)));calls=0;
        const auto independent=router.dispatch(route,[&](auto,const auto&){++calls;grants.values[target]->receive=false;return true;});
        require(std::get<adc::ConnectionDispatchReport>(independent).queued==2&&calls==2,"Target permission update canceled independent echo");
    }
    grants.values.fill(Permissions{});grants.values[2].reset();
    require(std::get<adc::ConnectionRoutingError>(router.prepare(id(1),request()))==adc::ConnectionRoutingError::forbidden,"Missing grant defaulted to guest access");
    SecuritySettings disabled;disabled.ctm_enabled=false;Core locked(sessions,disabled,&grants);adc::ConnectionRouter locked_router(locked);grants.values.fill(Permissions{});
    require(std::get<adc::ConnectionRoutingError>(locked_router.prepare(id(1),request()))==adc::ConnectionRoutingError::disabled,"Trusted grant bypassed operation disable");
    disabled={};disabled.guests.initiate=false;Core guests(sessions,disabled);adc::ConnectionRouter guest_router(guests);
    require(std::get<adc::ConnectionRoutingError>(guest_router.prepare(id(1),request()))==adc::ConnectionRoutingError::forbidden,"Null authority bypassed guest policy");
}
std::string drain(adc::LoginHub& hub,unsigned who) {
    std::string out;while(!hub.output(who).empty()&&!hub.must_close(who)){const std::string part(hub.output(who));out+=part;hub.consume(who,part.size());}return out;
}
std::string join(adc::LoginHub& hub,unsigned who,bool active) {
    hub.connect(who,"127.0.0.1");hub.receive(who,parse("HSUP ADBASE ADTIGR"));hub.consume(who,hub.output(who).size());
    const std::string sid(hub.output(who).substr(5,4));hub.consume(who,hub.output(who).size());
    adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(who);
    hub.receive(who,parse("BINF "+sid+" NIUser"+std::to_string(who)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)+" I40.0.0.0"+(active?" SUTCP4":"")));
    (void)drain(hub,who);return sid;
}
void managed_grants() {
    adc::LoginLimits limits;limits.transfers.guests.initiate=false;adc::LoginHub hub(limits);
    require(!hub.set_transfer_permissions(1,{}),"Missing lifetime granted");
    const auto source=join(hub,1,true),target=join(hub,2,false);(void)drain(hub,1);
    const auto wire="ECTM "+source+" "+target+" ADCS/1.0 01515 SecurityPrivate";
    hub.receive(1,parse(wire));require(drain(hub,1)=="ISTA 125 Connection\\soperation\\snot\\spermitted FCECTM\n"&&hub.output(2).empty(),"Guest policy leaked an echo or target request");
    const auto metadata="BINF "+source+" SUTCP4,ADCS CT1 APAdmin";
    hub.receive(1,parse(metadata));(void)drain(hub,1);(void)drain(hub,2);
    hub.receive(1,parse(wire));require(drain(hub,1).starts_with("ISTA 125 ")&&hub.output(2).empty(),"Public metadata granted authority");
    require(hub.set_transfer_permissions(1,{true,false})&&hub.set_transfer_permissions(2,{false,false}),"Trusted update failed");
    hub.receive(1,parse(wire));require(drain(hub,1).starts_with("ISTA 125 ")&&hub.output(2).empty(),"Recipient permission ignored");
    require(hub.set_transfer_permissions(2,{false,true}),"Target grant failed");hub.receive(1,parse(wire));
    require(drain(hub,1)==wire+'\n'&&drain(hub,2)==wire+'\n',"Authorized request lost original intent");
    hub.disconnect(1);(void)drain(hub,2);require(!hub.set_transfer_permissions(1,{}),"Departed lifetime granted");
    const auto fresh=join(hub,3,true);(void)drain(hub,2);
    hub.receive(3,parse("DCTM "+fresh+" "+target+" ADC/1.0 1515 SecurityPrivate"));
    require(drain(hub,3).starts_with("ISTA 125 ")&&hub.output(2).empty(),"Replacement inherited grant");
    hub.connect(4,"127.0.0.1");require(!hub.set_transfer_permissions(4,{}),"Negotiating lifetime granted");hub.disconnect(4,false);
}
}
int main(){try{addresses_ports_and_sids();authorization_matrix();revalidation_and_echo();managed_grants();std::cout<<"Connection security checks passed\n";}catch(const std::exception& e){deny_allocations=false;std::cerr<<e.what()<<'\n';return 1;}}
