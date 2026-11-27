#include "dc24h/users/limits.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
long allocation_budget=-1;
void* allocate(std::size_t size) {
    if (allocation_budget==0) { throw std::bad_alloc(); }
    if (allocation_budget>0) { --allocation_budget; }
    if (auto* memory=std::malloc(size==0?1:size)) { return memory; }
    throw std::bad_alloc();
}
}
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory,std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory,std::size_t) noexcept { std::free(memory); }

namespace {
using namespace dc24h;
void require(bool valid,const char* why) { if (!valid) { throw std::runtime_error(why); } }
template<class Operation> void invalid(Operation operation) {
    try { operation(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Invalid limit accepted");
}
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
users::User user(std::string_view suffix) {
    return *users::User::create(*users::Identity::create("S"+std::string(suffix),"C"+std::string(suffix),"N"+std::string(suffix)));
}
bool has_error(users::MutationResult result,users::RegistryError expected) {
    return result && std::get_if<users::RegistryError>(&*result) && std::get<users::RegistryError>(*result)==expected;
}
void add(users::SessionManager& sessions,std::uint64_t owner) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(owner),users::Transport::adcs,{})),"Session rejected below ceiling");
    require(!sessions.start_negotiation(id(owner),{}),"Negotiation failed");
}
void bounds() {
    require(users::Limits{}.bounded_by(1024)==users::Limits{1024,1024},"Default caps changed");
    require(users::Limits{4,8}.bounded_by(2)==users::Limits{2,2} &&
        users::Limits{4,1}.bounded_by(8)==users::Limits{4,1},"Effective cap precedence wrong");
    for (const auto limit : {users::Limits{0,1},users::Limits{1,0},users::Limits{65536,1},users::Limits{1,65536}}) {
        invalid([&] { users::validate(limit); });
        invalid([&] { users::SessionManager sessions(limit); });
        invalid([&] { (void)limit.bounded_by(1); }); // Invalid raw value cannot hide behind a smaller cap.
        invalid([&] { adc::LoginLimits options;options.users=limit;adc::LoginHub hub(options); });
    }
    invalid([] { (void)users::Limits{}.bounded_by(0); });
    invalid([] { (void)users::Limits{}.bounded_by(65536); });
    users::SessionManager low(users::Limits{1,65535}),high(users::Limits{65535,65535});
    require(low.capacity()==1 && low.user_capacity()==1 && high.user_capacity()==65535,"Boundary capacity rejected");
    users::SessionManager legacy(7);require(legacy.capacity()==7 && legacy.user_capacity()==7,"Legacy constructor changed");
}
void lifetime_capacity() {
    users::SessionManager sessions(users::Limits{3,1});add(sessions,1);add(sessions,2);add(sessions,3);
    require(std::get<users::RegistryError>(sessions.add(id(4),users::Transport::adc,{}))==users::RegistryError::capacity_reached,
        "Session ceiling ignored for pending models");
    require(!sessions.identify(id(1),user("1"),{}),"First user rejected");
    const auto candidate_before=*sessions.snapshot(id(2));
    for (const auto phase : {users::Phase::identified,users::Phase::ready,users::Phase::closing,users::Phase::closed}) {
        if (phase==users::Phase::ready) { require(!sessions.mark_ready(id(1),{}),"Ready failed"); }
        if (phase==users::Phase::closing) { require(!sessions.begin_close(id(1),users::CloseCause::login_timeout,{}),"Close failed"); }
        if (phase==users::Phase::closed) { require(!sessions.finish_close(id(1),users::CloseCause::peer_closed,{}),"Finish failed"); }
        auto candidate=user("2");allocation_budget=0;
        const auto rejected=sessions.identify(id(2),std::move(candidate),{});allocation_budget=-1;
        require(has_error(rejected,users::RegistryError::user_capacity_reached) && sessions.users_full() &&
            sessions.snapshot(id(2))==candidate_before && !sessions.by_sid("S2") && !sessions.by_cid("C2") && !sessions.by_nickname("N2"),
            "User cap allocated, released early or partially registered");
        require(std::get<users::RegistryError>(sessions.add(id(4),users::Transport::adc,{}))==users::RegistryError::capacity_reached,
            "Closing/closed session released before cleanup");
    }
    require(!sessions.remove(id(1)) && !sessions.users_full(),"Cleanup failed to return capacity");
    add(sessions,4); // A capacity refusal did not consume this local ID.
    require(!sessions.identify(id(2),user("2"),{}) && sessions.identified_users()==1,"Freed user slot not reusable");
    require(!sessions.rename(id(2),"Renamed",{}) && sessions.users_full(),"Rename at capacity changed slot ownership");
    require(has_error(sessions.identify(id(3),user("2"),{}),users::RegistryError::duplicate_sid),"Collision precedence changed at capacity");
}
void allocation_rollback() {
    bool failed=false,succeeded=false;
    for (long budget=0;budget<40 && !succeeded;++budget) {
        users::SessionManager sessions(users::Limits{3,2});add(sessions,1);add(sessions,2);add(sessions,3);
        require(!sessions.identify(id(1),user("1"),{}),"Sentinel registration failed");
        const auto before=*sessions.snapshot(id(2));auto candidate=user("2");
        allocation_budget=budget;
        try { const auto result=sessions.identify(id(2),std::move(candidate),{});allocation_budget=-1;
            require(!result,"Unexpected typed registration error");succeeded=true;
        } catch (const std::bad_alloc&) {
            allocation_budget=-1;failed=true;
            require(!sessions.users_full() && sessions.identified_users()==1 && sessions.snapshot(id(2))==before &&
                sessions.by_sid("S1") && !sessions.by_sid("S2") && !sessions.by_cid("C2") && !sessions.by_nickname("N2"),
                "Allocation failure leaked a user slot or mutated another owner");
            require(!sessions.identify(id(2),user("2"),{}),"Retry could not reuse uncommitted slot");
        }
        require(sessions.users_full() && has_error(sessions.identify(id(3),user("3"),{}),users::RegistryError::user_capacity_reached),
            "Post-registration capacity is inconsistent");
    }
    require(failed && succeeded,"Registration allocation campaign missed failure or success");
}
adc::Message parse(std::string_view text) { return std::get<adc::Message>(adc::parse_line(text)); }
std::vector<std::string> drain(adc::LoginHub& hub,std::uint64_t owner) {
    std::vector<std::string> frames;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) {
        frames.emplace_back(hub.output(owner));hub.consume(owner,frames.back().size());
    }
    return frames;
}
std::string identify(adc::LoginHub& hub,std::uint64_t owner) {
    hub.connect(owner,"127.0.0.1");hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size());const std::string sid(hub.output(owner).substr(5,4));hub.consume(owner,hub.output(owner).size());
    adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)));
    return sid;
}
void login_preflight() {
    adc::LoginLimits options;options.connections=8;options.users={4,1};adc::LoginHub hub(options);
    const auto first=identify(hub,1);
    require(!hub.ready(1) && hub.users()==1 && hub.sessions().capacity()==4,"Pending welcome not counted");
    const auto pinned=hub.output(1);const auto* pointer=pinned.data();
    for (std::uint64_t owner=2;owner<=3;++owner) {
        if (owner==3) { hub.close(1,adc::CloseReason::login_timeout); }
        const auto rejected_sid=identify(hub,owner);const auto frames=drain(hub,owner);
        require(frames.size()==2 && frames[0]=="ISTA 211 Hub\\suser\\slimit\\sreached\n" &&
            frames[1].starts_with("IQUI "+rejected_sid+" ") && hub.users()==1 && !hub.snapshot(owner)->identity,
            "Full hub leaked welcome/list or registered rejected user");
        require(hub.output(1).data()==pointer && hub.output(1)==pinned,"Rejection changed unrelated pinned front");
        hub.disconnect(owner); // Rejected/anonymous model emits no public QUI.
    }
    hub.disconnect(1,false);const auto fourth=identify(hub,4);const auto welcome=drain(hub,4);
    require(hub.ready(4) && hub.users()==1 && welcome.size()==3 && welcome[1].starts_with("BINF "+fourth+" ") && welcome.back().starts_with("IMSG Welcome") && fourth!=first,
        "Post-cleanup admission failed or included a rejected/old user");
    hub.disconnect(4,false);require(hub.queued_bytes()==0 && hub.users()==0 && hub.sessions().size()==0,"Cleanup retained limits");
}
}
int main() {
    try { bounds();lifetime_capacity();allocation_rollback();login_preflight();
        std::cout<<"PASS: bounded session/user ceilings, lifetime accounting, transactional slots and pre-welcome rejection\n";
    } catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
