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
    if (!value) { throw std::runtime_error("Kick assertion at line " + std::to_string(where.line())); }
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

void hierarchy_and_command() {
    using admin::KickError;
    auto settings=limits();settings.roles=security::parse_vip_permissions("users.kick");
    LoginHub hub(settings);
    const auto reg=login(hub,1,1),mod=login(hub,2,2);
    (void)login(hub,3,3);(void)login(hub,4,4);(void)login(hub,5,5);const auto owner=login(hub,6,6);const auto guest=login(hub,8,88);
    clear(hub,{1,2,3,4,5,6,8});
    for(const auto& pair:{std::pair{1U,8U},std::pair{2U,3U},std::pair{2U,2U},std::pair{5U,6U},std::pair{8U,1U}}) {
        require(hub.kick(pair.first,pair.second)==KickError::forbidden);
    }
    require(hub.kick(6,1,"bad\nreason")==KickError::invalid_reason);
    require(hub.kick(6,1,std::string(257,'x'))==KickError::invalid_reason);
    require(hub.kick(6,1,"e\xcc\x81")==KickError::invalid_reason);
    require(hub.kick(6,1,{},users::TimePoint{})==KickError::invalid_time);
    require(hub.ready(1)&&hub.ready(8));
    // Pending authentication targets are never eligible.
    hub.connect(9,"127.0.0.1",users::Transport::adcs);hub.transport_ready(9);hub.receive(9,parse("HSUP ADBASE ADTIGR"));(void)drain(hub,9);require(hub.kick(6,9)==KickError::unavailable);
    hub.disconnect(9,false);clear(hub,{1,2,3,4,5,6,8});
    // Search windows disappear at kick commit, before output/transport cleanup.
    hub.receive(1,parse("DSCH "+reg+" "+mod+" ANKickTest TOKickToken"));
    require(hub.search_statistics().tracked_searches==1);clear(hub,{1,2,3,4,5,6,8});
    hub.receive(4,parse("BMSG "+hub.session_model(4).identity()->sid()+" +kick\\sRole1\\sPrivate\\sreason"));
    require(hub.closing(1)&&!hub.authenticated_session(1)&&hub.search_statistics().tracked_searches==0);
    const auto notice=drain(hub,1);
    require(notice.find("ISTA 230 Private\\sreason\n")!=std::string::npos);
    require(notice.ends_with("IQUI "+reg+" ID"+hub.session_model(4).identity()->sid()+" TL0 MSPrivate\\sreason\n"));
    require(drain(hub,4).starts_with("ISTA 000 User\\skicked"));
    require(drain(hub,2).empty()&&drain(hub,8).empty());
    hub.receive(1,parse("BMSG "+reg+" Suppressed"));require(drain(hub,2).empty());
    require(hub.kick(4,1)==KickError::unavailable);
    hub.disconnect(1);require(drain(hub,2)=="IQUI "+reg+"\n");clear(hub,{3,4,5,6,8});
    // ID reuse at the trusted API boundary is already forbidden by registry.
    require(hub.kick(4,1)==KickError::unavailable);
    hub.receive(6,parse("BMSG "+owner+" +kick\\sGuest88"));
    require(hub.closing(8)&&drain(hub,8).find("IQUI "+guest)!=std::string::npos);
    require(drain(hub,2).empty());
    (void)mod;
}
void source_and_flood() {
    auto settings=limits();settings.anti_flood=chat::FloodLimits{1,1,16384,16384};
    LoginHub hub(settings);const auto mod=login(hub,2,2);const auto guest=login(hub,8,88);clear(hub,{2,8});
    // Denied and malformed reserved commands are never broadcast.
    hub.receive(8,parse("BMSG "+guest+" +kick\\sRole2"));
    require(drain(hub,8).starts_with("ISTA 125 Kick\\sdenied"));require(drain(hub,2).empty());
    hub.receive(2,parse("BMSG "+mod+" +kick"));require(drain(hub,8).empty());(void)drain(hub,2);
    hub.receive(2,parse("BMSG "+mod+" +kick\\sGuest88"));
    require(hub.ready(8)&&drain(hub,2).find("rate\\slimit")!=std::string::npos);
    LoginHub spoof(limits());const auto user_sid=login(spoof,1,1);const auto operator_sid=login(spoof,2,2);(void)login(spoof,8,88);clear(spoof,{1,2,8});
    spoof.receive(1,parse("BMSG "+operator_sid+" +kick\\sGuest88"));
    require(spoof.closing(1)&&spoof.ready(8)&&spoof.kick_statistics().values[0]==0);
    (void)user_sid;
}
void pinned_output_and_failures() {
    using admin::KickError;
    for(int fault=-1;fault<48;++fault){
        LoginHub hub(limits());const auto mod=login(hub,2,2),guest=login(hub,8,88);clear(hub,{2,8});
        hub.receive(8,parse("BMSG "+guest+" Pinned"));(void)hub.consume(8,1);(void)drain(hub,2);
        const auto remaining=hub.output(8);const auto* pointer=remaining.data();const auto size=remaining.size();
        const auto snapshot=hub.snapshot(8);
        fail_after=fault;const auto result=hub.kick(2,8,"Reason");fail_after=-1;
        if(result==KickError::resources){require(hub.snapshot(8)==snapshot&&hub.ready(8)&&hub.kick_statistics().values[3]==1);}
        else {
            require(result==KickError::none&&!hub.ready(8)&&hub.kick_statistics().values[1]==1);
            require(hub.output(8).data()==pointer&&hub.output(8).size()==size);
            if(!hub.session_model(8).immediate()){require(drain(hub,8).ends_with("IQUI "+guest+" ID"+mod+" TL0 MSReason\n"));}
        }
        require(hub.kick_statistics().values[0]==1);
    }
}
void queue_cap_and_chat_disabled() {
    using admin::KickError;
    auto settings=limits();settings.output_per_client=512;settings.output_total=4096;settings.chat.public_enabled=false;
    LoginHub hub(settings);const auto mod=login(hub,2,2),guest=login(hub,8,88);clear(hub,{2,8});
    const auto queued=hub.send_hub_message(std::string(350,'x'),*users::SessionId::from_value(8));
    require(std::holds_alternative<chat::DispatchReport>(queued));
    require(hub.kick(2,8,std::string(256,'x'))==KickError::none);
    require(hub.must_close(8)&&hub.ready(2)&&hub.kick_statistics().values[4]==1);
    // Administrative capability is independent of ordinary chat publication.
    LoginHub command(settings);const auto sender=login(command,2,2);(void)login(command,8,88);clear(command,{2,8});
    command.receive(2,parse("BMSG "+sender+" +kick\\sGuest88"));
    require(command.closing(8)&&command.kick_statistics().values[1]==1);
    require(command.chat_statistics().values[0]==0);
    (void)mod;(void)guest;
}

}
int main(){try{hierarchy_and_command();source_and_flood();pinned_output_and_failures();queue_cap_and_chat_disabled();std::cout<<"Live kick hierarchy, private commands and queue safety passed\n";}
catch(const std::exception& e){fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}}
