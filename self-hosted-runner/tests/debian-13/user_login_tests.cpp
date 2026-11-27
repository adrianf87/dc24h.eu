#include "dc24h/adc/user_login.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
long allocation_budget = -1;
void* allocate(std::size_t bytes) {
    if (allocation_budget == 0) { throw std::bad_alloc(); }
    if (allocation_budget > 0) { --allocation_budget; }
    if (auto* memory = std::malloc(bytes == 0 ? 1 : bytes)) { return memory; }
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
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
adc::Message parse(std::string_view wire) { return std::get<adc::Message>(adc::parse_line(wire)); }
template<class Error>
bool error(adc::UserLoginResult result, Error expected) {
    return result && std::get_if<Error>(&*result) && std::get<Error>(*result) == expected;
}
adc::Message identity(std::string_view sid, unsigned seed, std::string_view nickname) {
    adc::TigerDigest pid{}; pid[0] = static_cast<std::uint8_t>(seed);
    return parse("BINF " + std::string(sid) + " NI" + std::string(nickname) + " ID" +
        adc::tiger_base32(*adc::tiger192(pid)) + " PD" + adc::tiger_base32(pid) + " DEPublic\\sprofile APncdc LOpl_PL");
}
void handshake(adc::ClientSession& client) {
    require(!client.observe(adc::Direction::from_client,parse("HSUP ADBASE ADTIGR")), "SUP rejected");
    require(!client.observe(adc::Direction::to_client,*client.support().initial_reply()) && !client.prepare_sid(), "SUP/SID preparation failed");
    require(!client.observe(adc::Direction::to_client,*client.sid_reply()), "SID reply rejected");
}
void approve(adc::ClientSession& client, unsigned seed, std::string_view nickname) {
    handshake(client);
    require(!client.observe(adc::Direction::from_client,identity(client.sid(),seed,nickname)), "Valid INF rejected");
    require(!client.approve_identity(adc::IdentityMode::guest), "Guest approval rejected");
}
void add(users::SessionManager& sessions, std::uint64_t value) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(value),users::Transport::adc,{})), "Session add failed");
    require(!sessions.start_negotiation(id(value),users::TimePoint{}+1ms), "Negotiation failed");
}
void gates_and_completion() {
    users::SessionManager sessions(3); adc::UserLogin login(sessions);
    auto pool = std::make_shared<adc::SidPool>(); adc::ClientSession client(pool,"127.0.0.1");
    require(error(login.register_guest(id(99),client,{}),users::RegistryError::missing), "Missing session accepted");
    add(sessions,1); const auto before = *sessions.snapshot(id(1));
    require(error(login.register_guest(id(1),client,users::TimePoint{}+2ms),adc::UserLoginError::invalid_protocol_state), "Unnegotiated protocol registered");
    handshake(client);
    auto invalid = identity(client.sid(),1,"Żółw");
    for (auto& token : invalid.parameters) { if (token.starts_with("PD")) { token = "PD" + std::string(39,'A'); } }
    require(client.observe(adc::Direction::from_client,invalid).has_value() && !client.info(), "Invalid identity retained");
    require(error(login.register_guest(id(1),client,users::TimePoint{}+2ms),adc::UserLoginError::invalid_protocol_state) &&
        sessions.snapshot(id(1)) == before && sessions.identified_users() == 0, "Invalid proof registered a user");
    require(!client.observe(adc::Direction::from_client,identity(client.sid(),1,"Żółw")), "Valid proof rejected");
    require(error(login.register_guest(id(1),client,users::TimePoint{}+2ms),adc::UserLoginError::invalid_protocol_state), "Identity without explicit approval registered");
    require(!client.approve_identity(adc::IdentityMode::guest), "Guest approval failed");
    require(error(login.register_guest(id(1),client,{}),users::SessionError::backwards_time) && sessions.snapshot(id(1)) == before,
        "Backwards admission changed the registry");
    require(!login.register_guest(id(1),client,users::TimePoint{}+2ms), "Verified guest not registered");
    const auto admitted = *sessions.snapshot(id(1));
    const auto cid = std::string(*client.info()->field("ID"));
    require(admitted.phase == users::Phase::identified && !admitted.timeline.ready && sessions.identified_users() == 1 &&
        sessions.user_by_sid(client.sid()) == sessions.user_by_cid(cid) && !client.info()->field("PD"), "Admission exposed wrong readiness/public identity");
    require(error(login.register_guest(id(1),client,users::TimePoint{}+3ms),users::SessionError::invalid_state) &&
        sessions.snapshot(id(1)) == admitted, "Repeated registration changed the model");
    adc::ClientSession other(pool,"127.0.0.1"); approve(other,2,"Other");
    require(error(login.complete_welcome(id(1),other,users::TimePoint{}+3ms),adc::UserLoginError::identity_mismatch) &&
        sessions.snapshot(id(1)) == admitted, "Unrelated protocol session completed welcome");
    require(!client.observe(adc::Direction::from_client,parse("BINF " + std::string(client.sid()) + " NIChanged")), "Fixture update failed");
    require(error(login.complete_welcome(id(1),client,users::TimePoint{}+3ms),adc::UserLoginError::identity_mismatch), "Stale nickname completed welcome");
    require(!client.observe(adc::Direction::from_client,parse("BINF " + std::string(client.sid()) + " NIŻółw")), "Fixture restore failed");
    require(error(login.complete_welcome(id(1),client,{}),users::SessionError::backwards_time), "Backwards welcome accepted");
    allocation_budget = 0;
    const auto completed = login.complete_welcome(id(1),client,users::TimePoint{}+3ms);
    allocation_budget = -1;
    require(!completed && sessions.find(id(1))->ready(), "Welcome completion allocated or failed");
    const auto ready = *sessions.snapshot(id(1));
    require(error(login.complete_welcome(id(1),client,users::TimePoint{}+4ms),users::SessionError::invalid_state) &&
        sessions.snapshot(id(1)) == ready, "Welcome completed twice");
    require(!sessions.begin_close(id(1),users::CloseCause::shutdown,users::TimePoint{}+4ms), "Close failed");
    require(error(login.register_guest(id(1),client,users::TimePoint{}+5ms),users::SessionError::invalid_state) &&
        error(login.complete_welcome(id(1),client,users::TimePoint{}+5ms),users::SessionError::invalid_state), "Closing session logged in");
    require(!sessions.finish_close(id(1),users::CloseCause::shutdown,users::TimePoint{}+5ms) && !sessions.remove(id(1)) &&
        !sessions.by_sid(client.sid()) && !sessions.by_cid(cid), "Cleanup retained login indexes");
}
void collisions() {
    users::SessionManager sessions(4); adc::UserLogin login(sessions); auto pool = std::make_shared<adc::SidPool>();
    adc::ClientSession first(pool,"127.0.0.1"), duplicate_cid(pool,"127.0.0.1"), duplicate_nick(pool,"127.0.0.1");
    approve(first,1,"Taken"); approve(duplicate_cid,1,"Different"); approve(duplicate_nick,2,"Taken");
    add(sessions,1); add(sessions,2); add(sessions,3); add(sessions,4);
    require(!login.register_guest(id(1),first,users::TimePoint{}+2ms), "First admission failed");
    const auto before = *sessions.snapshot(id(2));
    require(error(login.register_guest(id(2),duplicate_cid,users::TimePoint{}+2ms),users::RegistryError::duplicate_cid) &&
        error(login.register_guest(id(3),duplicate_nick,users::TimePoint{}+2ms),users::RegistryError::duplicate_nickname) &&
        sessions.snapshot(id(2)) == before && sessions.identified_users() == 1 && !sessions.by_sid(duplicate_cid.sid()), "Collision changed public ownership");
    adc::ClientSession same_sid(std::make_shared<adc::SidPool>(),"127.0.0.1"); approve(same_sid,3,"Unique");
    require(error(login.register_guest(id(4),same_sid,users::TimePoint{}+2ms),users::RegistryError::duplicate_sid), "Duplicate SID admitted");
    require(adc::user_login_status(users::RegistryError::duplicate_cid) == "224" &&
        adc::user_login_status(users::RegistryError::duplicate_nickname) == "222" &&
        adc::user_login_status(users::RegistryError::missing) == "210" &&
        adc::user_login_status(users::SessionError::invalid_state) == "244" &&
        adc::user_login_status(adc::UserLoginError::unverified_identity) == "227", "Typed login status mapping changed");
}
void allocation_failures() {
    bool success = false;
    for (long budget = 0; budget < 64 && !success; ++budget) {
        users::SessionManager sessions(1); adc::UserLogin login(sessions); add(sessions,1);
        adc::ClientSession client(std::make_shared<adc::SidPool>(),"127.0.0.1"); approve(client,8,"AllocationGuest");
        const auto before = *sessions.snapshot(id(1)); const auto info = *client.info();
        const auto sid = std::string(client.sid()), cid = std::string(*info.field("ID"));
        allocation_budget = budget;
        try { const auto result = login.register_guest(id(1),client,users::TimePoint{}+2ms); allocation_budget = -1;
            require(!result, "Unexpected allocation campaign error"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1;
            require(sessions.snapshot(id(1)) == before && sessions.identified_users() == 0 && !sessions.by_sid(sid) &&
                !sessions.by_cid(cid) && !sessions.by_nickname("AllocationGuest") && *client.info() == info &&
                client.state() == adc::ConnectionState::normal, "Projection/registration allocation left partial login state");
            require(!login.register_guest(id(1),client,users::TimePoint{}+2ms), "Allocation failure blocked retry");
        }
        require(sessions.identified_users() == 1 && !sessions.find(id(1))->ready() &&
            !login.complete_welcome(id(1),client,users::TimePoint{}+3ms), "Recovered login did not complete");
    }
    require(success, "Login allocation campaign never completed");
}
void output_boundary() {
    adc::LoginHub hub; hub.connect(1,"127.0.0.1"); hub.receive(1,parse("HSUP ADBASE ADTIGR"));
    require(!hub.consume(1,hub.output(1).size()), "SUP marked ready");
    const std::string sidline(hub.output(1)); require(!hub.consume(1,sidline.size()), "SID marked ready");
    const auto sid = sidline.substr(5,4); hub.receive(1,identity(sid,7,"Boundary"));
    require(hub.users() == 1 && !hub.ready(1) && hub.sessions().user_by_sid(sid), "Queued welcome not registered/ready ordering");
    require(!hub.consume(1,hub.output(1).size()), "Hub INF marked ready");
    const auto length = hub.output(1).size();
    require(!hub.consume(1,length-1) && !hub.ready(1) && hub.snapshot(1)->phase == users::Phase::identified, "Partial own INF marked ready");
    require(hub.consume(1,1) && hub.ready(1), "Own INF final LF did not mark ready");
    hub.disconnect(1); require(hub.users() == 0 && !hub.snapshot(1), "Welcome cleanup failed");
    adc::LoginHub limited({2,128,256}); limited.connect(1,"127.0.0.1"); limited.receive(1,parse("HSUP ADBASE ADTIGR"));
    limited.consume(1,limited.output(1).size()); const std::string line(limited.output(1)); limited.consume(1,line.size());
    auto large = identity(line.substr(5,4),9,"Overflow");
    for (auto& token : large.parameters) { if (token.starts_with("AP")) { token = "AP" + std::string(100,'A'); } }
    limited.receive(1,large);
    require(limited.must_close(1) && !limited.ready(1) && limited.users() == 0 && !limited.session_model(1).identity(), "Output overflow registered a user");
    limited.disconnect(1); require(limited.queued_bytes() == 0, "Overflow output leaked");
}
void live_registration_allocation_failure() {
    bool isolated = false, success = false;
    for (long budget = 0; budget < 256 && !success; ++budget) {
        adc::LoginHub hub; hub.connect(1,"127.0.0.1"); hub.receive(1,parse("HSUP ADBASE ADTIGR"));
        hub.consume(1,hub.output(1).size()); const std::string line(hub.output(1)); hub.consume(1,line.size());
        const auto initial = identity(line.substr(5,4),5,"MemoryGuest");
        allocation_budget = budget;
        try { hub.receive(1,initial); allocation_budget = -1;
            if (hub.must_close(1)) {
                require(hub.session_model(1).terminal_cause() == users::CloseCause::internal_error &&
                    hub.users() == 0 && !hub.session_model(1).identity() && !hub.ready(1), "Registration allocation published/failed to close a guest");
                isolated = true;
            } else { require(hub.users() == 1, "Successful login missing registration"); success = true; }
        } catch (const std::bad_alloc&) { allocation_budget = -1;
            // Earlier parser/protocol/welcome allocations are outside the new
            // registration guard. They must still not publish a public user.
            require(hub.users() == 0 && !hub.session_model(1).identity(), "Pre-registration allocation published a guest");
        }
        hub.disconnect(1,false);
        require(hub.users() == 0 && hub.queued_bytes() == 0 && !hub.snapshot(1), "Allocation cleanup leaked registration/output");
    }
    require(isolated && success, "Live allocation campaign did not cover both guarded failure and success");
}
}
int main() {
    try { gates_and_completion(); collisions(); allocation_failures(); output_boundary(); live_registration_allocation_failure();
        std::cout << "PASS: verified guest registration, typed rejection, final-LF readiness and allocation rollback\n";
    } catch (const std::exception& error) { allocation_budget = -1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
