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
    if (!value) { throw std::runtime_error("Roles assertion at line " + std::to_string(where.line())); }
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
void matrix() {
    for (auto role : {Role::registered,Role::moderator,Role::administrator}) {
        for (auto permission : {Permission::search_queries,Permission::search_results,Permission::initiate_transfers,Permission::receive_transfers}) {
            require(security::allows(role,permission));
        }
        require(security::allows(role,Permission::revoke_sessions) == (role != Role::registered));
        require(security::allows(role,Permission::manage_accounts) == (role == Role::administrator));
        require(!security::allows(role,static_cast<Permission>(999)));
        for (auto target : {Role::registered,Role::moderator,Role::administrator}) {
            const auto expected = (role == Role::moderator && target == Role::registered) ||
                (role == Role::administrator && (target == Role::registered || target == Role::moderator));
            require(security::can_revoke(role,target) == expected);
            require(security::can_manage(role,target) == (role == Role::administrator && target != Role::administrator));
        }
        require(!security::can_revoke(role,static_cast<Role>(999)));
    }
    require(!security::allows(static_cast<Role>(999),Permission::search_queries));
    require(!security::can_revoke(static_cast<Role>(999),Role::registered));
    require(!security::can_manage(static_cast<Role>(999),Role::registered));
    require(!security::can_manage(Role::administrator,static_cast<Role>(999)));
    users::SessionManager manager(1); const auto id = *users::SessionId::from_value(1);
    require(std::holds_alternative<const users::Session*>(manager.add(id,users::Transport::adc,{})));
    require(!manager.start_negotiation(id,{}));
    require(!manager.identify(id,*users::User::create(*users::Identity::create("AAAA","CID","User")),{}));
    const auto before = manager.snapshot(id);
    require(manager.mark_authenticated_ready(id,*accounts::AccountId::from_value(1),{},static_cast<Role>(999)).has_value());
    require(manager.snapshot(id) == before);
}
void detailed_permissions() {
    using security::parse_permission;using security::permission_key;
    for(unsigned i=0;i<accounts::role_catalog.size();++i){
        const auto role=accounts::role_catalog[i];require(security::role_rank(role)==i);
        for(auto permission:security::permission_catalog){
            require(parse_permission(permission_key(permission))==permission);
            const auto key=permission_key(permission);
            const bool basic=key=="hub.use"||key=="chat.public"||key=="chat.private"||key.starts_with("search.")||key.starts_with("transfers.");
            const bool moderation=key=="users.kick"||key=="users.mute"||key=="audit.read";
            const bool operation=key=="users.ban"||key=="blocks.manage";
            const bool administration=key=="accounts.manage"||key=="roles.manage"||key=="configuration.manage";
            require(security::allows(role,permission)==(basic||(i>=3&&moderation)||(i>=4&&operation)||(i>=5&&administration)||(i>=1&&key=="accounts.password.change")||i==6));
        }
        for(unsigned j=0;j<accounts::role_catalog.size();++j){
            const auto target=accounts::role_catalog[j];
            require(security::can_revoke(role,target)==(i>=3&&i>j));
            require(security::can_mute(role,target)==(i>=3&&i>j));
            require(security::can_manage(role,target)==(i>=5&&j>0&&i>j));
            for(unsigned k=0;k<accounts::role_catalog.size();++k)
                require(security::can_assign(role,target,accounts::role_catalog[k])==(i>=5&&j>0&&i>j&&k>0&&i>k));
        }
    }
    const auto vip=security::parse_vip_permissions("users.kick, audit.read,users.mute");
    require(security::allows(Role::vip,Permission::revoke_sessions,vip)&&security::can_revoke(Role::vip,Role::registered,vip));
    require(!security::can_revoke(Role::vip,Role::vip,vip)&&!security::allows(Role::vip,Permission::manage_accounts,vip));
    for(const auto text:{"","*","none,users.kick","users.ban","roles.manage","accounts.manage","users.kick,users.kick","users.kick,","USERS.KICK"}){
        bool rejected=false;try{(void)security::parse_vip_permissions(text);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    }
    require(!parse_permission("users.Kick")&&!parse_permission("users.kick ")&&permission_key(static_cast<Permission>(999)).empty());
    require(!security::allows(Role::owner,Permission::manage_accounts,security::RolePolicy{0xffffffffU}));
    fail_allocation=true;
    const bool allocation_free=security::allows(Role::owner,Permission::manage_roles)&&security::can_manage(Role::owner,Role::administrator)&&security::can_revoke(Role::vip,Role::guest,vip);
    fail_allocation=false;require(allocation_free);
    users::SessionManager sessions(1);const auto id=*users::SessionId::from_value(1);
    require(std::holds_alternative<const users::Session*>(sessions.add(id,users::Transport::adc,{})));
    require(!sessions.start_negotiation(id,{}));
    require(!sessions.identify(id,*users::User::create(*users::Identity::create("AAAA","CID","User")),{}));
    const auto before=sessions.snapshot(id);
    const auto refused=sessions.mark_authenticated_ready(id,*accounts::AccountId::from_value(1),{},Role::guest);
    require(refused&&std::holds_alternative<users::SessionError>(*refused)&&std::get<users::SessionError>(*refused)==users::SessionError::invalid_role);
    require(sessions.snapshot(id)==before);
    require(!sessions.mark_authenticated_ready(id,*accounts::AccountId::from_value(1),{},Role::registered));
}
void expanded_session_roles() {
    auto settings=limits();auto source=std::make_shared<Source>();
    source->roles[0]=Role::vip;source->roles[1]=Role::operator_;source->roles[2]=Role::owner;
    settings.authentication=source;settings.roles=security::parse_vip_permissions("users.kick");
    LoginHub hub(settings);(void)login(hub,1,1);(void)login(hub,2,2);(void)login(hub,3,3);(void)login(hub,4,4);(void)login(hub,8,88);clear(hub,{1,2,3,4,8});
    require(hub.authenticated_session(1)->role==Role::vip&&hub.authenticated_session(2)->role==Role::operator_&&hub.authenticated_session(3)->role==Role::owner);
    require(hub.role_allows(1,Permission::revoke_sessions)&&!hub.role_allows(1,Permission::ban_users));
    require(hub.role_allows(2,Permission::ban_users)&&!hub.role_allows(2,Permission::manage_accounts));
    require(hub.role_allows(3,Permission::manage_roles));
    require(!hub.revoke_auth_session_as(1,2)&&!hub.revoke_auth_session_as(2,3));
    require(hub.revoke_auth_session_as(1,4)&&hub.revoke_auth_session_as(2,8));
    require(!hub.authenticated_session(4)&&hub.must_close(8));
}
void readiness() {
    auto options = limits(); auto source = std::make_shared<Source>(); options.authentication = source;
    LoginHub hub(options); const auto sid = start(hub,1,3);
    require(!hub.role_allows(1,Permission::revoke_sessions));
    while (!hub.output(1).starts_with("BINF "+sid+" ")) { require(!hub.output(1).empty()); (void)hub.consume(1,hub.output(1).size()); }
    (void)hub.consume(1,hub.output(1).size()-1);
    require(!hub.authenticated_session(1) && !hub.role_allows(1,Permission::revoke_sessions));
    require(hub.consume(1,1)); (void)drain(hub,1);
    require(hub.authenticated_session(1)->role == Role::administrator && hub.role_allows(1,Permission::revoke_sessions));
    source->roles[2] = Role::registered; // Live records do not follow directory mutations.
    require(hub.authenticated_session(1)->role == Role::administrator);
    hub.close(1,CloseReason::idle_timeout);
    require(!hub.role_allows(1,Permission::revoke_sessions) && !hub.authenticated_session(1)); hub.disconnect(1,false);
    (void)login(hub,2,88); require(hub.role_allows(2,Permission::hub_use)&&hub.role_allows(2,Permission::search_queries));
    require(!hub.role_allows(2,Permission::manage_accounts));hub.disconnect(2,false);
    (void)start(hub,3,7); require(hub.closing(3) && !hub.authenticated_session(3)); hub.disconnect(3,false);
}
void routing() {
    LoginHub hub(limits()); const auto first = login(hub,1,1), target = login(hub,2,4), guest = login(hub,3,88);
    clear(hub,{1,2,3});
    const auto query = "DSCH "+first+" "+target+" ANRoleQuery TORoleToken";
    hub.receive(1,parse(query)); require(drain(hub,2) == query+"\n" && drain(hub,1).empty());
    hub.receive(3,parse("DSCH "+guest+" "+target+" ANDenied"));
    require(drain(hub,3).starts_with("ISTA 125 Search\\soperation\\snot\\spermitted")); require(hub.output(2).empty());
    const auto result = "DRES "+target+" "+first+" FN/file SI0 TR"+std::string(39,'A')+" TORoleToken";
    hub.receive(2,parse(result)); require(drain(hub,1) == result+"\n");
    const auto transfer = "DCTM "+first+" "+target+" ADC/1.0 1511 RoleTransfer";
    hub.receive(1,parse(transfer)); require(drain(hub,2) == transfer+"\n");
    hub.receive(3,parse("DCTM "+guest+" "+target+" ADC/1.0 1511 Denied"));
    require(drain(hub,3).starts_with("ISTA 125 Connection\\soperation\\snot\\spermitted"));
    require(hub.set_search_permissions(1,{false,false}));
    require(hub.search_statistics().tracked_searches == 0);
    hub.receive(1,parse("HSUP ADOPER"));
    hub.receive(1,parse("BINF "+first+" DERoleProfile")); clear(hub,{1,2,3});
    require(hub.authenticated_session(1)->role == Role::registered);
    hub.receive(1,parse(query)); require(drain(hub,1).starts_with("ISTA 125 Search\\soperation\\snot\\spermitted"));
    require(hub.set_transfer_permissions(2,{true,false}));
    hub.receive(2,parse("BINF "+target+" DERoleProfile")); clear(hub,{1,2,3});
    hub.receive(1,parse(transfer));
    require(drain(hub,1).starts_with("ISTA 125 Connection\\soperation\\snot\\spermitted"));
    require(hub.output(2).empty());
    auto disabled = limits(); disabled.search.queries_enabled = false; disabled.transfers.ctm_enabled = false;
    LoginHub blocked(disabled); const auto admin = login(blocked,1,3), receiver = login(blocked,2,4); clear(blocked,{1,2});
    blocked.receive(1,parse("DSCH "+admin+" "+receiver+" ANDisabled"));
    require(drain(blocked,1).starts_with("ISTA 125 Search\\soperation\\sis\\sdisabled"));
    blocked.receive(1,parse("DCTM "+admin+" "+receiver+" ADC/1.0 1511 Disabled"));
    require(drain(blocked,1).starts_with("ISTA 125 Connection\\soperation\\sis\\sdisabled") && blocked.output(2).empty());
}
void moderation() {
    LoginHub hub(limits());
    for (unsigned seed = 1; seed <= 6; ++seed) { (void)login(hub,seed,seed); }
    (void)login(hub,8,88); clear(hub,{1,2,3,4,5,6,8});
    for (const auto& pair : {std::pair{1U,2U},std::pair{2U,3U},std::pair{2U,5U},std::pair{3U,6U},
                           std::pair{3U,3U},std::pair{0U,1U},std::pair{3U,99U},std::pair{8U,1U}}) {
        require(!hub.revoke_auth_session_as(pair.first,pair.second));
    }
    require(!hub.revoke_auth_session_as(3,2,users::TimePoint{}));
    const auto snapshot = hub.authenticated_session(4);
    fail_allocation = true; const bool revoked = hub.revoke_auth_session_as(2,4); fail_allocation = false;
    require(revoked && hub.must_close(4) && !hub.authenticated_session(4) && hub.ready(2));
    require(!hub.revoke_auth_session_as(2,4));
    require(hub.revoke_auth_session_as(3,5) && !hub.revoke_auth_session_as(5,1));
    hub.disconnect(4,false); (void)login(hub,9,4);
    require(!hub.revoke_auth_session_as(2,4) && hub.ready(9) && snapshot->role == Role::registered);
    require(hub.revoke_auth_session_as(2,1));
    require(!hub.role_allows(1,Permission::search_queries));
}
}
int main() {
    try { matrix(); detailed_permissions(); expanded_session_roles(); readiness(); routing(); moderation(); std::cout << "Role matrix, trusted grants, policy precedence and moderation checks passed\n"; }
    catch (const std::exception& error) { fail_allocation = false; std::cerr << error.what() << '\n'; return 1; }
}
