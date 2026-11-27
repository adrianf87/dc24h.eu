#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/security/roles.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <source_location>
#include <stdexcept>

namespace { int fail_after = -1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_after == 0) { fail_after=-1; throw std::bad_alloc(); }
    if (fail_after>0) { --fail_after; }
    if (auto* value = std::malloc(size ? size : 1)) { return value; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
extern "C" int __wrap_RAND_bytes(unsigned char* data, int size) {
    for (int i = 0; i < size; ++i) { data[i] = static_cast<unsigned char>(i); }
    return 1;
}
namespace {
using namespace dc24h;
using namespace dc24h::adc;
using accounts::Role;
using security::Permission;
constexpr std::string_view password = "ADC wire password 42!";
constexpr std::string_view response = "5P4QWFGBOLYMZ6DFRPDKS3FV7Y27NF7XSSZ4QNQ";
void require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("Mute assertion at line " + std::to_string(where.line())); }
}
Message parse(std::string_view text) { return std::get<Message>(parse_line(text)); }
TigerDigest pid(unsigned seed) {
    TigerDigest value{};
    for (std::size_t i = 0; i < value.size(); ++i) { value[i] = static_cast<unsigned char>(i+seed); }
    return value;
}
std::string cid(unsigned seed) { return tiger_base32(*tiger192(pid(seed))); }
struct Source final : AuthenticationSource {
    std::array<Role,7> roles{Role::registered,Role::moderator,Role::administrator,
        Role::vip,Role::operator_,Role::owner,static_cast<Role>(999)};
    AuthenticationPreparation prepare(std::string_view nick, std::string_view supplied,
        users::Transport, PasswordChallenge& challenge) const override {
        for (unsigned i = 1; i <= roles.size(); ++i) {
            if (nick == "Role"+std::to_string(i) || supplied == cid(i)) {
                if (nick != "Role"+std::to_string(i) || supplied != cid(i)) { return DeniedAuthentication{}; }
                return PasswordAuthentication{i,std::get<Message>(challenge.issue(password,SessionHash::tiger192)),roles[i-1]};
            }
        }
        return GuestAuthentication{};
    }
    bool allows_identity(std::string_view, std::string_view, std::optional<std::uint64_t>) const noexcept override { return true; }
};
std::string drain(LoginHub& hub, std::uint64_t id) {
    std::string output;
    while (!hub.output(id).empty()) { const auto wire = hub.output(id); output += wire; (void)hub.consume(id,wire.size()); }
    return output;
}
void clear(LoginHub& hub, std::initializer_list<std::uint64_t> ids) { for (auto id : ids) { (void)drain(hub,id); } }
std::string start(LoginHub& hub, std::uint64_t id, unsigned seed) {
    hub.connect(id,"127.0.0.1",users::Transport::adcs); hub.transport_ready(id);
    hub.receive(id,parse("HSUP ADBASE ADTIGR")); const auto welcome = drain(hub,id);
    const auto sid = welcome.substr(welcome.find("ISID ")+5,4);
    hub.receive(id,parse("BINF "+sid+" NI"+(seed < 8 ? "Role" : "Guest")+std::to_string(seed)+
        " ID"+cid(seed)+" PD"+tiger_base32(pid(seed))+" I40.0.0.0 SUTCP4"));
    if (seed < 7) {
        require(drain(hub,id).starts_with("IGPA "));
        hub.receive(id,parse("HPAS "+std::string(response)));
    }
    return sid;
}
std::string login(LoginHub& hub, std::uint64_t id, unsigned seed) {
    const auto sid = start(hub,id,seed); (void)drain(hub,id); require(hub.ready(id)); return sid;
}
LoginLimits limits() {
    LoginLimits value; value.authentication = std::make_shared<Source>();
    value.search.guests = {false,false}; value.transfers.guests = {false,false}; return value;
}

void hierarchy(){
    using admin::MuteError;using admin::MuteScope;
    auto settings=limits();settings.roles=security::parse_vip_permissions("users.mute");LoginHub hub(settings);
    for(unsigned n=1;n<=6;++n){(void)login(hub,n,n);}(void)login(hub,8,88);clear(hub,{1,2,3,4,5,6,8});
    for(const auto& pair:{std::pair{1U,8U},std::pair{2U,3U},std::pair{2U,2U},std::pair{5U,6U},std::pair{8U,1U}}){
        require(hub.mute(pair.first,pair.second,MuteScope::all,true)==MuteError::forbidden);
        require(hub.mute(pair.first,pair.second,MuteScope::all,false)==MuteError::forbidden);
    }
    require(hub.mute(6,1,static_cast<MuteScope>(99),true)==MuteError::invalid_scope);
    require(hub.mute(6,1,MuteScope::all,true,{})==MuteError::invalid_time&&!hub.chat_mute(1)->public_chat);
    require(hub.mute(6,99,MuteScope::all,true)==MuteError::unavailable);
    require(hub.mute(4,1,MuteScope::public_chat,true)==MuteError::none);
    require(hub.chat_mute(1)==users::ChatMute{true,false});
    require(hub.kick(4,1)==admin::KickError::forbidden); // A mute delegation is not a kick grant.
    require(hub.mute(4,1,MuteScope::all,false)==MuteError::none&&hub.chat_mute(1)==users::ChatMute{});
    require(hub.mute(6,2,MuteScope::all,true)==MuteError::none);
    require(hub.mute(2,1,MuteScope::all,true)==MuteError::none); // Muted staff retain moderation authority.
    hub.connect(9,"127.0.0.1",users::Transport::adcs);hub.transport_ready(9);
    require(hub.mute(6,9,MuteScope::all,true)==MuteError::unavailable);
    require(hub.mute(9,1,MuteScope::all,true)==MuteError::forbidden);
    hub.disconnect(9,false);hub.disconnect(2,false);
    require(hub.mute(2,1,MuteScope::all,true)==MuteError::forbidden);
    settings.roles=security::parse_vip_permissions("users.kick");LoginHub restricted(settings);
    (void)login(restricted,1,1);(void)login(restricted,4,4);
    require(restricted.mute(4,1,MuteScope::all,true)==MuteError::forbidden);
    for(auto scope:{MuteScope::public_chat,MuteScope::private_chat,MuteScope::all})require(admin::parse_mute_scope(admin::mute_scope_key(scope))==scope);
    require(!admin::parse_mute_scope("both")&&!admin::parse_mute_scope("PUBLIC"));
}
void routing(){
    using admin::MuteError;using admin::MuteScope;LoginHub hub(limits());
    const auto sender=login(hub,1,1),actor=login(hub,2,2),witness=login(hub,3,88);clear(hub,{1,2,3});
    require(hub.mute(2,1,MuteScope::public_chat,true)==MuteError::none);require(drain(hub,1).starts_with("ISTA 000 Public\\schat\\smuted"));
    for(const auto flags:{""," ME1"," ME0"})hub.receive(1,parse("BMSG "+sender+" SecretPublic"+flags));
    require(drain(hub,1).find("Public\\schat\\sis\\smuted")!=std::string::npos&&drain(hub,2).empty()&&drain(hub,3).empty());
    hub.receive(1,parse("EMSG "+sender+" "+witness+" PrivateAllowed PM"+sender+" ME1"));
    require(drain(hub,1).find("PrivateAllowed")!=std::string::npos&&drain(hub,3).find("PrivateAllowed")!=std::string::npos);
    require(hub.mute(2,1,MuteScope::private_chat,true)==MuteError::none&&hub.chat_mute(1)==users::ChatMute{true,true});clear(hub,{1,2,3});
    for(const auto kind:{"DMSG","EMSG"})hub.receive(1,parse(std::string(kind)+" "+sender+" "+witness+" SecretPrivate PM"+sender+" ME1"));
    hub.receive(1,parse("EMSG "+sender+" "+sender+" SecretSelf"));
    hub.receive(1,parse("DMSG "+sender+" ZZZZ UnknownRecipient"));
    const auto denied=drain(hub,1);require(denied.find("Secret")==std::string::npos&&denied.find("recipient")==std::string::npos);
    require(drain(hub,3).empty()&&drain(hub,2).empty());
    hub.receive(3,parse("DMSG "+witness+" "+sender+" IncomingAllowed"));require(drain(hub,1).find("IncomingAllowed")!=std::string::npos);
    (void)hub.send_hub_message("HubAllowed",*users::SessionId::from_value(1));require(drain(hub,1).find("HubAllowed")!=std::string::npos);
    hub.receive(1,parse("DSCH "+sender+" "+actor+" ANMuteQuery TOMuteToken"));require(hub.search_statistics().tracked_searches==1);clear(hub,{1,2,3});
    require(hub.mute(2,1,MuteScope::public_chat,false)==MuteError::none&&hub.chat_mute(1)==users::ChatMute{false,true});clear(hub,{1,2,3});
    hub.receive(1,parse("BMSG "+sender+" PublicRestored"));require(drain(hub,3).find("PublicRestored")!=std::string::npos);clear(hub,{1,2,3});
    require(hub.ready(1)&&hub.authenticated_session(1).has_value()&&hub.mute_statistics().values[6]==3&&hub.mute_statistics().values[7]==4);
    hub.disconnect(1,false);clear(hub,{2,3});(void)login(hub,9,1);require(hub.chat_mute(9)==users::ChatMute{});
}
void commands(){
    LoginHub hub(limits());const auto reg=login(hub,1,1),mod=login(hub,2,2);const auto guest=login(hub,3,88);clear(hub,{1,2,3});
    const auto before=hub.chat_statistics();
    hub.receive(2,parse("BMSG "+mod+" +mute\\sRole1\\spublic"));
    require(hub.chat_mute(1)==users::ChatMute{true,false}&&hub.chat_statistics()==before);
    require(drain(hub,2).starts_with("ISTA 000 User\\smuted")&&drain(hub,1).starts_with("ISTA 000 Public\\schat\\smuted")&&drain(hub,3).empty());
    hub.receive(2,parse("BMSG "+mod+" +mute\\sRole1"));require(hub.chat_mute(1)==users::ChatMute{true,true});clear(hub,{1,2,3});
    hub.receive(2,parse("BMSG "+mod+" +unmute\\sRole1\\sprivate"));require(hub.chat_mute(1)==users::ChatMute{true,false});clear(hub,{1,2,3});
    for(const auto command:{"+mute","+unmute ","+mute Role1 both","+unmute Role1 all extra","+mute Role1 all ME1"}){
        // Encode spaces in command text; ME1 separately exercises forbidden flags below.
        std::string encoded;for(char c:std::string_view(command)){encoded+=c==' '?"\\s":std::string(1,c);}
        hub.receive(2,parse("BMSG "+mod+" "+encoded));require(drain(hub,2).starts_with("ISTA 100 Mute\\srefused"));require(drain(hub,1).empty()&&drain(hub,3).empty());
    }
    hub.receive(2,parse("BMSG "+mod+" +unmute\\sRole1 ME1"));require(drain(hub,2).starts_with("ISTA 100")&&hub.chat_mute(1)->public_chat);
    hub.receive(1,parse("BMSG "+reg+" +mute\\sGuest88"));require(drain(hub,1).starts_with("ISTA 125 Mute\\sdenied"));require(!hub.chat_mute(3)->public_chat);
    hub.receive(2,parse("DMSG "+mod+" "+guest+" +mute\\sRole1"));require(drain(hub,3).find("+mute\\sRole1")!=std::string::npos);
    // A nickname update cannot clear a session's mute; closing does clear it.
    require(hub.mute(2,3,admin::MuteScope::all,true)==admin::MuteError::none);clear(hub,{1,2,3});
    hub.receive(3,parse("BINF "+guest+" NIRenamedGuest"));require(hub.chat_mute(3)==users::ChatMute{true,true});clear(hub,{1,2,3});
    hub.receive(2,parse("BMSG "+mod+" +unmute\\sRenamedGuest"));require(hub.chat_mute(3)==users::ChatMute{});clear(hub,{1,2,3});
    // Spoofed source is a protocol failure before moderation runs.
    const auto attempts=hub.mute_statistics().values[0];hub.receive(3,parse("BMSG "+mod+" +mute\\sRole1"));require(hub.closing(3)&&hub.mute_statistics().values[0]==attempts);
}
void plans_and_lifetime(){
    users::SessionManager sessions(4);const auto a=*users::SessionId::from_value(1),b=*users::SessionId::from_value(2);
    for(auto id:{a,b}){const auto key="SID"+std::to_string(id.value());require(std::holds_alternative<const users::Session*>(sessions.add(id,users::Transport::adc,{})));
        require(!sessions.start_negotiation(id,{})&&!sessions.identify(id,*users::User::create(*users::Identity::create(key,"CID"+key,"Nick"+key)),{}));
        if(id==b){require(!sessions.mark_authenticated_ready(id,*accounts::AccountId::from_value(2),{},Role::moderator));}
        else{require(!sessions.mark_ready(id,{}));}}
    admin::MuteCore moderation(sessions,{});
    chat::Core core(sessions);auto pub=std::get<chat::Broadcast>(core.prepare(a,"SID1","Prepared"));
    auto pm=std::get<chat::Directed>(core.prepare_direct(a,"SID1","SID2","Prepared",{},true));unsigned calls=0;
    const chat::Core::Sink public_sink=[&](auto,const auto&){++calls;return true;};const chat::Core::DirectedSink private_sink=[&](auto,const auto&){++calls;return true;};
    fail_after=0;const auto changed=moderation.apply(b,a,admin::MuteScope::all,true,{});const auto blocked=core.dispatch(pub,public_sink),blocked_pm=core.dispatch(pm,private_sink);fail_after=-1;
    require(changed==admin::MuteError::none&&calls==0&&std::get<chat::Error>(blocked)==chat::Error::muted&&std::get<chat::Error>(blocked_pm)==chat::Error::muted);
    require(std::get<chat::Error>(core.prepare(a,"SID1","Blocked"))==chat::Error::muted);
    require(std::get<chat::Error>(core.prepare_direct(a,"SID1","missing","Blocked"))==chat::Error::muted);
    require(std::get<chat::Error>(core.prepare(a,"SID2","Spoofed"))==chat::Error::wrong_source);
    require(!sessions.set_chat_mute(a,{false,true},{}));require(std::holds_alternative<chat::DispatchReport>(core.dispatch(pub,public_sink))&&calls==2);
    const auto snapshot=sessions.snapshot(a);require(snapshot->chat_mute==users::ChatMute{false,true});
    const auto future=users::TimePoint{}+std::chrono::seconds(2);
    require(moderation.apply(b,a,admin::MuteScope::private_chat,true,future)==admin::MuteError::none);
    require(moderation.apply(b,a,admin::MuteScope::all,false,future-std::chrono::seconds(1))==admin::MuteError::invalid_time);
    require(sessions.find(a)->chat_mute()==users::ChatMute{false,true});
    require(!sessions.begin_close(a,users::CloseCause::peer_closed,future)&&sessions.find(a)->chat_mute()==users::ChatMute{});
    require(sessions.set_chat_mute(a,{true,true},{}).has_value());
}
void allocation_and_flood(){
    using admin::MuteError;using admin::MuteScope;bool before=false,after=false;
    for(int n=0;n<60&&!(before&&after);++n){LoginHub hub(limits());(void)login(hub,1,1);(void)login(hub,2,2);clear(hub,{1,2});
        fail_after=n;const auto result=hub.mute(2,1,MuteScope::all,true);fail_after=-1;
        if(result==MuteError::resources){before=true;require(hub.chat_mute(1)==users::ChatMute{}&&hub.mute_statistics().values[1]==0);}
        else{require(result==MuteError::none&&hub.mute_statistics().values[1]==1);if(hub.mute_statistics().values[5])after=true;}
    }require(before&&after);
    auto settings=limits();settings.anti_flood=chat::FloodLimits{};settings.anti_flood->message_burst=1;
    LoginHub hub(settings);const auto reg=login(hub,1,1),mod=login(hub,2,2);clear(hub,{1,2});
    hub.receive(2,parse("BMSG "+mod+" +mute\\sRole1"));clear(hub,{1,2});
    hub.receive(2,parse("BMSG "+mod+" +unmute\\sRole1"));require(hub.chat_mute(1)==users::ChatMute{true,true});
    require(drain(hub,2).find("rate\\slimit")!=std::string::npos&&hub.mute_statistics().values[0]==1);(void)reg;
}
}
int main(){try{hierarchy();routing();commands();plans_and_lifetime();allocation_and_flood();std::cout<<"Mute scope, hierarchy, routing, lifetime, allocation and flood checks passed\n";}
catch(const std::exception& e){fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}}
