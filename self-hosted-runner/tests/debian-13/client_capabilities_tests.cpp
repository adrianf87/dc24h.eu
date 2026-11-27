#include "dc24h/adc/connection_routing.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace { bool forbid_allocations = false; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (forbid_allocations) { throw std::bad_alloc(); }
    if (auto* memory = std::malloc(size ? size : 1)) { return memory; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace dc24h;
using namespace dc24h::transfers;
void require(bool result, const char* reason) { if (!result) { throw std::runtime_error(reason); } }
users::SessionId id(unsigned n) { return *users::SessionId::from_value(n); }
users::Profile profile(unsigned mask, std::string v4="127.0.0.1", std::string v6="::1") {
    users::ProfileData data; data.ipv4=std::move(v4); data.ipv6=std::move(v6);
    const std::array<std::string, 5> names{"TCP4", "TCP6", "UDP4", "UDP6", "ADCS"};
    for (unsigned bit=0; bit<names.size(); ++bit) { if (mask & (1U << bit)) { data.features.push_back(names[bit]); } }
    data.udp4_port=65535; data.udp6_port=1;
    return *users::Profile::create(std::move(data));
}
void add(users::SessionManager& sessions, unsigned n, users::Profile p, bool ready=true) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n), n%2 ? users::Transport::adc : users::Transport::adcs, {})), "Add failed");
    require(!sessions.start_negotiation(id(n), {}), "Negotiation failed");
    const auto sid = n==1 ? "AAAA" : "BBBB";
    require(!sessions.identify(id(n), *users::User::create(*users::Identity::create(sid, "CID"+std::to_string(n), "User"+std::to_string(n)), std::move(p)), {}), "Identify failed");
    if (ready) { require(!sessions.mark_ready(id(n), {}), "Ready failed"); }
}
void update(users::SessionManager& sessions, unsigned n, users::Profile p) {
    require(!sessions.update_user(id(n), *users::User::create(*sessions.find(id(n))->identity(), std::move(p)), {}), "Update failed");
}
void capability_matrix() {
    static_assert(std::is_trivially_copyable_v<ClientCapabilities>);
    for (unsigned a=0; a<32; ++a) for (unsigned b=0; b<32; ++b) {
        auto origin_profile=profile(a), peer_profile=profile(b);
        forbid_allocations=true;
        const auto origin=client_capabilities(origin_profile), peer=client_capabilities(peer_profile);
        const auto choice=select_connection(origin, peer);
        for (auto family : {AddressFamily::ipv4, AddressFamily::ipv6}) {
            const unsigned bit = family==AddressFamily::ipv4 ? 1U : 2U;
            require((origin.mode(family)==TcpMode::active)==bool(a&bit), "TCP family conflated with UDP/TLS");
            require(connection_address(origin,peer,Kind::connect_to_me,family).has_value()==bool(a&bit), "CTM role matrix differs");
            require(connection_address(origin,peer,Kind::reverse_connect,family).has_value()==(!(a&bit) && bool(b&bit)), "RCM role matrix differs");
        }
        require(origin.protocols.adc && origin.protocols.adcs==bool(a&16), "TLS declaration inferred from mode");
        if (a&3) {
            require(choice && choice->kind==Kind::connect_to_me && choice->family==((a&1)?AddressFamily::ipv4:AddressFamily::ipv6), "Originator CTM preference differs");
        } else if (b&3) {
            require(choice && choice->kind==Kind::reverse_connect && choice->family==((b&1)?AddressFamily::ipv4:AddressFamily::ipv6), "Originator RCM preference differs");
        } else { require(!choice, "Passive/passive acquired a direct path"); }
        require(!select_connection(origin,peer,static_cast<Kind>(99)) &&
                !connection_address(origin,peer,Kind::connect_to_me,static_cast<AddressFamily>(99)), "Invalid native enum accepted");
        forbid_allocations=false;
        // Snapshots own their fixed address bytes and survive profile replacement.
        origin_profile=profile(0); peer_profile=profile(0);
        require(origin.tcp4.has_value()==bool(a&1) && peer.tcp6.has_value()==bool(b&2), "Snapshot borrowed profile storage");
    }
    for (const auto& address : {"", "0.0.0.0", "224.0.0.1", "255.255.255.255"}) {
        require(!client_capabilities(profile(31,address)).tcp4, "Unusable IPv4 listener admitted");
    }
    for (const auto& address : {"", "::", "ff02::1", "fe80::1", "::ffff:127.0.0.1"}) {
        require(!client_capabilities(profile(31,"127.0.0.1",address)).tcp6, "Unusable IPv6 listener admitted");
    }
    users::ProfileData data; data.ipv4="127.0.0.1"; data.features={"TCPX","ADC0","ADCS","UDP4"}; data.udp4_port=1515;
    const auto caps=client_capabilities(*users::Profile::create(data));
    require(!caps.tcp4 && !caps.tcp6 && caps.protocols.adcs, "Unknown/legacy/UDP feature implied TCP activity");
}
void live_plans() {
    users::SessionManager sessions(4); Core core(sessions); adc::ConnectionRouter router(core);
    require(!core.capabilities(id(1)), "Missing session has capabilities");
    add(sessions,1,profile(0)); add(sessions,2,profile(3),false);
    require(!core.capabilities(id(2)), "Welcome-pending capabilities exposed");
    require(!sessions.mark_ready(id(2),{}), "Ready failed");
    const auto old_snapshot=*core.capabilities(id(2));
    for (bool reverse : {false,true}) for (bool echo : {false,true}) {
        update(sessions,1,profile(reverse?0:3)); update(sessions,2,profile(3));
        const auto wire=std::string(echo?"E":"D")+(reverse?"RCM":"CTM")+" AAAA BBBB ADCS/1.0 "+(reverse?"":"1515 ")+"CapabilityPrivate";
        const auto message=std::get<adc::Message>(adc::parse_line(wire));
        auto prepared=router.prepare(id(1),message);
        require(std::holds_alternative<adc::RoutedConnection>(prepared), "Path refused");
        const auto route=std::get<adc::RoutedConnection>(std::move(prepared));
        require(route.plan().request().family()==AddressFamily::ipv4, "Initial family differs");
        // An unrelated-family update preserves the selected path.
        update(sessions,reverse?2:1,profile(1));
        unsigned calls=0;
        const adc::ConnectionRouter::Sink sink=[&](auto, const auto&) { ++calls; return true; };
        forbid_allocations=true; const auto report=router.dispatch(route,sink); forbid_allocations=false;
        require(std::get<adc::ConnectionDispatchReport>(report).queued==(echo?2U:1U) && calls==(echo?2U:1U), "Unrelated family canceled dispatch or allocated");
        auto next=router.prepare(id(1),message); const auto stale=std::get<adc::RoutedConnection>(std::move(next));
        // Removing selected activity must cancel instead of switching to IPv6.
        update(sessions,reverse?2:1,profile(2));
        calls=0; require(std::holds_alternative<adc::ConnectionRoutingError>(router.dispatch(stale,sink)) && !calls, "Stale plan switched family");
        const auto fresh=std::get<adc::RoutedConnection>(router.prepare(id(1),message));
        require(fresh.plan().request().family()==AddressFamily::ipv6, "Fresh request did not use new capabilities");
    }
    require(old_snapshot.tcp4 && old_snapshot.tcp6, "Ready-session snapshot changed after INF");
    update(sessions,1,profile(0)); update(sessions,2,profile(1));
    auto prepared=router.prepare(id(1),std::get<adc::Message>(adc::parse_line("DRCM AAAA BBBB ADC/1.0 ChangeRole")));
    const auto stale=std::get<adc::RoutedConnection>(std::move(prepared));
    update(sessions,1,profile(1));
    unsigned calls=0;
    require(std::holds_alternative<adc::ConnectionRoutingError>(router.dispatch(stale,[&](auto,const auto&){++calls;return true;})) && !calls, "Now-active RCM source dispatched");
}
}
int main() {
    try { capability_matrix(); live_plans(); std::cout << "Client capability checks passed\n"; }
    catch (const std::exception& error) { forbid_allocations=false; std::cerr << error.what() << '\n'; return 1; }
}
