#include "dc24h/adc/inf_updates.hpp"
#include "dc24h/search/core.hpp"
#include "dc24h/adc/user_login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/serializer.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
long allocation_budget = -1;
void* allocate(std::size_t bytes) {
    if (allocation_budget == 0) { throw std::bad_alloc(); }
    if (allocation_budget > 0) { --allocation_budget; }
    if (auto* value = std::malloc(bytes == 0 ? 1 : bytes)) { return value; }
    throw std::bad_alloc();
}
}
void* operator new(std::size_t bytes) { return allocate(bytes); }
void* operator new[](std::size_t bytes) { return allocate(bytes); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {
using namespace dc24h;
using namespace std::chrono_literals;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t number) { return *users::SessionId::from_value(number); }
adc::Message parse(std::string_view line) { return std::get<adc::Message>(adc::parse_line(line)); }
struct Fixture {
    users::SessionManager sessions{4};
    adc::UserLogin login{sessions};
    adc::InfUpdates updates{sessions};
    std::shared_ptr<adc::SidPool> pool = std::make_shared<adc::SidPool>();
    std::map<std::uint64_t,std::unique_ptr<adc::ClientSession>> clients;
    adc::ClientSession& add(std::uint64_t number, bool ready = true) {
        (void)sessions.add(id(number),users::Transport::adc,{});
        require(!sessions.start_negotiation(id(number),{}),"Negotiation failed");
        auto client = std::make_unique<adc::ClientSession>(pool,"127.0.0.1");
        require(!client->observe(adc::Direction::from_client,parse("HSUP ADBASE ADTIGR")) &&
            !client->observe(adc::Direction::to_client,*client->support().initial_reply()) && !client->prepare_sid() &&
            !client->observe(adc::Direction::to_client,*client->sid_reply()),"SUP/SID failed");
        adc::TigerDigest pid{}; pid[0]=static_cast<std::uint8_t>(number);
        require(!client->observe(adc::Direction::from_client,parse("BINF "+std::string(client->sid())+" NIUser"+std::to_string(number)+
            " ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)+
            " DEOriginal EMold@example.test SS1 SF2 SL3 SUTCP4 I40.0.0.0")) &&
            !client->approve_identity(adc::IdentityMode::guest) && !login.register_guest(id(number),*client,users::TimePoint{}+1ms),"Admission failed");
        if (ready) { require(!login.complete_welcome(id(number),*client,users::TimePoint{}+2ms),"Ready failed"); }
        auto* result=client.get();clients.emplace(number,std::move(client));return *result;
    }
    adc::InfUpdateResult apply(std::string_view fields, users::TimePoint now = users::TimePoint{}+3ms) {
        auto& client=*clients.at(1);
        return updates.apply(id(1),client,parse("BINF "+std::string(client.sid())+std::string(fields)),now);
    }
};

void mode_matrix() {
    using search::AddressFamily; using search::ReplyRoute;
    users::ProfileData data; data.ipv4="127.0.0.1";data.ipv6="::1";data.udp4_port=1;data.udp6_port=65535;
    for (unsigned mask=0;mask<16;++mask) {
        data.features.clear();
        const std::array<std::string,4> names{"TCP4","TCP6","UDP4","UDP6"};
        for (unsigned bit=0;bit<4;++bit) { if (mask & (1U<<bit)) { data.features.push_back(names[bit]); } }
        const auto profile=*users::Profile::create(data);
        allocation_budget=0;const auto paths=search::reply_paths(profile);allocation_budget=-1;
        require(paths.tcp4==bool(mask&1) && paths.tcp6==bool(mask&2) && paths.udp4.has_value()==bool(mask&4) && paths.udp6.has_value()==bool(mask&8),"TCP/UDP family activity conflated or allocated");
        require(paths.route(AddressFamily::ipv4)==((mask&4)?ReplyRoute::udp4:ReplyRoute::hub) &&
            paths.route(AddressFamily::ipv6)==((mask&8)?ReplyRoute::udp6:ReplyRoute::hub),"Family reply path differs");
        if (paths.udp4) { require(paths.udp4->port==1 && paths.udp4->address[0]==127 && paths.udp4->address[3]==1 && paths.udp4->address[15]==0,"IPv4 endpoint bytes differ"); }
        if (paths.udp6) { require(paths.udp6->port==65535 && paths.udp6->address[15]==1,"IPv6 endpoint bytes differ"); }
    }
    data.features={"TCP4","TCP6","UDP4","UDP6"};
    for (const auto port:{std::optional<std::int64_t>{},std::optional<std::int64_t>{0}}) {
        data.udp4_port=port;data.udp6_port=port;
        const auto paths=search::reply_paths(*users::Profile::create(data));
        require(paths.tcp4 && paths.tcp6 && !paths.udp4 && !paths.udp6,"Absent/zero UDP port implied TCP passivity or UDP activity");
    }
    for (const auto port:{-1,65536}) { data.udp4_port=port;require(!users::Profile::create(data),"Invalid UDP port narrowed before validation"); }
    data.udp4_port=1234;data.udp6_port=1234;
    for (const auto* address:{"", "0.0.0.0", "224.0.0.1", "255.255.255.255"}) {
        data.ipv4=address;require(!search::reply_paths(*users::Profile::create(data)).udp4,"Non-unicast/absent IPv4 produced UDP endpoint");
    }
    for (const auto* address:{"", "::", "ff02::1", "fe80::1", "::ffff:127.0.0.1"}) {
        data.ipv6=address;require(!search::reply_paths(*users::Profile::create(data)).udp6,"Unusable IPv6 produced UDP endpoint");
    }
    data.ipv4="127.0.0.1";data.ipv6="::1";data.features={"UDP5","XUDP"};
    require(!search::reply_paths(*users::Profile::create(data)).udp4,"Unknown/substr feature enabled UDP");
    for (const auto* bad:{"localhost", "127.0.0.1:2", "::1", "127.0.0.999"}) {
        data.ipv4=bad;require(!users::Profile::create(data),"Malformed/wrong-family IPv4 accepted");
    }
    data.ipv4="127.0.0.1";data.ipv6="fe80::1%lo";require(!users::Profile::create(data),"Scoped IPv6 was silently reinterpreted");
}
void projection_updates_and_snapshots() {
    Fixture fixture;auto& client=fixture.add(1);fixture.add(2);
    search::Core core(fixture.sessions);
    require(core.reply_paths(id(1))->tcp4 && !core.reply_paths(id(1))->udp4 && !core.reply_paths(id(99)),"Initial passive/ready model wrong");
    require(std::holds_alternative<adc::AppliedInfUpdate>(fixture.apply(" SUTCP4,UDP4 U41234 I40.0.0.0")),"Active update failed");
    auto paths=*core.reply_paths(id(1));
    require(paths.udp4 && paths.udp4->port==1234 && fixture.sessions.find(id(1))->user()->profile().data().ipv4=="127.0.0.1","Canonical address/port not projected");
    search::QueryData criteria;criteria.include={"active"};
    const auto plan=std::get<search::Plan>(core.prepare(id(1),client.sid(),criteria));
    require(plan.reply_paths()==paths,"Search admission did not capture owning reply metadata");
    require(std::holds_alternative<adc::AppliedInfUpdate>(fixture.apply(" SUUDP4 U465535",users::TimePoint{}+4ms)),"UDP-only update failed");
    require(!core.reply_paths(id(1))->tcp4 && core.reply_paths(id(1))->udp4->port==65535 && plan.reply_paths()==paths,"TCP flag conflated, port stale or plan mutated");
    require(std::holds_alternative<adc::AppliedInfUpdate>(fixture.apply(" I4",users::TimePoint{}+5ms)) && !core.reply_paths(id(1))->udp4,"Address cancellation left stale endpoint");
    require(std::holds_alternative<adc::AppliedInfUpdate>(fixture.apply(" I40.0.0.0 U40",users::TimePoint{}+6ms)) && !core.reply_paths(id(1))->udp4,"Zero port left active endpoint");
    require(std::holds_alternative<adc::AppliedInfUpdate>(fixture.apply(" U41",users::TimePoint{}+7ms)) && core.reply_paths(id(1))->udp4,"Positive port did not restore UDP");
    require(std::holds_alternative<adc::AppliedInfUpdate>(fixture.apply(" SU U4",users::TimePoint{}+8ms)) && !core.reply_paths(id(1))->udp4,"Feature/port cancellation left stale endpoint");
    const auto before=*client.info();const auto snapshot=*fixture.sessions.snapshot(id(1));
    for (const auto* invalid:{" I4192.0.2.123 U49 SUUDP4"," I6::1 U69 SUUDP6"," U465536 SUUDP4"," U6-1"," U41 U42"}) {
        require(std::holds_alternative<adc::InfUpdateFailure>(fixture.apply(invalid,users::TimePoint{}+9ms)) && *client.info()==before && fixture.sessions.snapshot(id(1))==snapshot,"Spoof/invalid port partially published network metadata");
    }
    allocation_budget=0;const auto current=core.reply_paths(id(1));allocation_budget=-1;
    require(current && !current->udp4,"Ready connectivity lookup allocated");
    require(!fixture.sessions.begin_close(id(1),users::CloseCause::shutdown,users::TimePoint{}+10ms) && !core.reply_paths(id(1)),"Closing owner exposed active paths");
}
void network_projection_faults() {
    Fixture fixture;auto& client=fixture.add(1);const auto before=*client.info();const auto snapshot=*fixture.sessions.snapshot(id(1));
    const auto input=parse("BINF "+std::string(client.sid())+" I40.0.0.0 U465535 SUTCP4,UDP4 DE"+std::string(100,'x'));
    bool success=false,failed=false;
    for (long budget=0;budget<512 && !success;++budget) {
        allocation_budget=budget;
        try { auto result=fixture.updates.apply(id(1),client,input,users::TimePoint{}+3ms);allocation_budget=-1;
            require(std::holds_alternative<adc::AppliedInfUpdate>(result),"Network fault became semantic refusal");success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1;failed=true;
            require(*client.info()==before && fixture.sessions.snapshot(id(1))==snapshot,"Network projection allocation failure partially committed");
        }
    }
    require(failed && success && search::reply_paths(fixture.sessions.find(id(1))->user()->profile()).udp4->port==65535,"Network projection fault campaign failed");
}
} // namespace
int main() {
    try { mode_matrix();projection_updates_and_snapshots();network_projection_faults();
        std::cout<<"PASS: independent active/passive families, endpoint projection, snapshots and faults\n";
    } catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
