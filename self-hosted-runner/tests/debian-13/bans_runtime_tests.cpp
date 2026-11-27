#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/security/roles.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <source_location>
#include <stdexcept>

namespace { bool fail_allocation = false; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_allocation) { throw std::bad_alloc(); }
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
    if (!value) { throw std::runtime_error("Ban admission assertion at line " + std::to_string(where.line())); }
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
        Role::registered,Role::moderator,Role::administrator,static_cast<Role>(999)};
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
[[maybe_unused]] void clear(LoginHub& hub, std::initializer_list<std::uint64_t> ids) { for (auto id : ids) { (void)drain(hub,id); } }
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
void admission() {
    const auto denied=[&](std::vector<bans::Rule> rules,unsigned seed,std::string_view code){
        auto settings=limits();settings.bans=std::make_shared<const bans::Directory>(rules);LoginHub hub(settings);
        const auto witness=login(hub,1,88);(void)witness;
        hub.connect(2,"127.0.0.1",users::Transport::adcs);hub.transport_ready(2);hub.receive(2,parse("HSUP ADBASE ADTIGR"));
        const auto greeting=drain(hub,2);const auto sid=greeting.substr(greeting.find("ISID ")+5,4);
        hub.receive(2,parse("BINF "+sid+" NI"+(seed<8?"Role":"Guest")+std::to_string(seed)+" ID"+cid(seed)+" PD"+tiger_base32(pid(seed))+" I40.0.0.0"));
        const auto output=drain(hub,2);require(output.starts_with(std::string("ISTA ")+std::string(code)+" Access\\sblocked"));
        require(output.find("IGPA ")==std::string::npos&&output.find("IQUI "+sid)!=std::string::npos&&hub.closing(2));
        require(drain(hub,1).empty()&&!hub.authenticated_session(2));
    };
    denied({{bans::Kind::account,"1",{}}},1,"231");
    denied({{bans::Kind::cid,cid(99),bans::Directory::now()+60}},99,"232");
    auto settings=limits();settings.bans=std::make_shared<const bans::Directory>(std::vector<bans::Rule>{{bans::Kind::account,"1",bans::Directory::now()-1}});
    LoginHub hub(settings);(void)login(hub,1,1);require(hub.ready(1));
}
}
int main(){try{admission();std::cout<<"Ban admission precedes account challenge and public presence\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
