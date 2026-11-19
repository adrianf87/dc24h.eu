#include "dc24h/adc/user_list.hpp"
#include "dc24h/adc/user_login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/serializer.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
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
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
adc::Message parse(std::string_view line) { return std::get<adc::Message>(adc::parse_line(line)); }
struct Fixture {
    users::SessionManager sessions{600};
    adc::UserLogin login{sessions};
    adc::UserList list{sessions};
    std::shared_ptr<adc::SidPool> pool = std::make_shared<adc::SidPool>();
    std::map<std::uint64_t,std::unique_ptr<adc::ClientSession>> clients;
    adc::ClientSession& add(std::uint64_t number, bool admitted = true) {
        (void)sessions.add(id(number),users::Transport::adc,{});
        require(!sessions.start_negotiation(id(number),{}), "Negotiation failed");
        auto client = std::make_unique<adc::ClientSession>(pool,"127.0.0.1");
        require(!client->observe(adc::Direction::from_client,parse("HSUP ADBASE ADTIGR")), "SUP failed");
        require(!client->observe(adc::Direction::to_client,*client->support().initial_reply()) && !client->prepare_sid() &&
            !client->observe(adc::Direction::to_client,*client->sid_reply()), "SID failed");
        adc::TigerDigest pid{}; pid[0] = static_cast<std::uint8_t>(number); pid[1] = static_cast<std::uint8_t>(number >> 8U);
        require(!client->observe(adc::Direction::from_client,parse("BINF " + std::string(client->sid()) + " NIUser" +
            std::to_string(number) + " ID" + adc::tiger_base32(*adc::tiger192(pid)) + " PD" + adc::tiger_base32(pid) +
            " DEŻółw I40.0.0.0 US1024 U41234 SUADC0,TCP4 XXprivate")), "INF failed");
        require(!client->approve_identity(adc::IdentityMode::guest), "Guest approval failed");
        if (admitted) { require(!login.register_guest(id(number),*client,users::TimePoint{}+1ms), "Admission failed"); }
        auto* result = client.get(); clients.emplace(number,std::move(client)); return *result;
    }
    const adc::ClientInfo* info(users::SessionId owner) const {
        const auto found = clients.find(owner.value());
        return found != clients.end() && found->second->info() ? &*found->second->info() : nullptr;
    }
    adc::UserListResult prepare(std::uint64_t joining, std::size_t bytes = 1024*1024) const {
        return list.prepare(id(joining),*clients.at(joining),[this](users::SessionId owner) { return info(owner); },bytes);
    }
};
void contract() {
    Fixture fixture;
    fixture.add(1); fixture.add(2); fixture.add(3); fixture.add(4);
    require(!fixture.sessions.mark_ready(id(2),users::TimePoint{}+2ms), "Ready failed");
    require(!fixture.sessions.begin_close(id(3),users::CloseCause::shutdown,users::TimePoint{}+2ms) &&
        !fixture.sessions.finish_close(id(4),users::CloseCause::shutdown,users::TimePoint{}+2ms), "Close failed");
    (void)fixture.sessions.add(id(5),users::Transport::adcs,{}); // No TLS/INF/user yet.
    auto& joining = fixture.add(6,false);
    const auto before = *fixture.sessions.snapshot(id(6));
    const auto all = fixture.sessions.public_users();
    require(all == std::vector<users::SessionId>{id(1),id(2)}, "Public membership included pending/closing/closed sessions");
    require(fixture.sessions.public_users(0,1) == std::vector<users::SessionId>{id(1)} &&
        fixture.sessions.public_users(1,1) == std::vector<users::SessionId>{id(2)} &&
        fixture.sessions.public_users(2,1).empty() && fixture.sessions.public_users(std::numeric_limits<std::uint64_t>::max()).empty(), "Membership cursor/order failed");
    for (const auto count : {std::size_t{0},users::SessionManager::max_page_size+1}) {
        bool rejected = false;
        try { (void)fixture.sessions.public_users(0,count); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid public page size accepted");
    }
    auto result = fixture.prepare(6);
    require(std::holds_alternative<adc::InitialUserList>(result), "Valid list rejected");
    const auto plan = std::get<adc::InitialUserList>(std::move(result));
    const auto& frames = plan.frames();
    require(frames.size() == 4 && frames.front().message.type == adc::MessageType::info && frames.front().message.kind == adc::Command::inf &&
        frames[1].message.source == fixture.clients.at(1)->sid() && frames[2].message.source == fixture.clients.at(2)->sid() &&
        frames.back().message.source == joining.sid(), "Welcome ordering or own-INF delimiter failed");
    std::size_t bytes = 0;
    for (const auto& frame : frames) {
        const auto wire = std::get<std::string>(adc::serialize(frame.message));
        require(frame.wire == wire, "Retained welcome encoding drifted");
        bytes += frame.wire.size();
        require(frame.wire.find(" PD") == std::string::npos && frame.wire.find(" XX") == std::string::npos, "List leaked private/unknown fields");
    }
    require(frames[1].wire.find("I4127.0.0.1") != std::string::npos &&
        frames[1].wire.find("US1024") != std::string::npos, "List lost validated ADC-only metadata");
    require(bytes == plan.wire_bytes() && std::holds_alternative<adc::InitialUserList>(fixture.prepare(6,bytes)) &&
        std::get<adc::UserListError>(fixture.prepare(6,bytes-1)) == adc::UserListError::output_limit &&
        std::get<adc::UserListError>(fixture.prepare(6,0)) == adc::UserListError::output_limit, "Exact aggregate wire budget failed");
    const auto missing = fixture.list.prepare(id(6),joining,[](users::SessionId) { return static_cast<const adc::ClientInfo*>(nullptr); },bytes);
    require(std::get<adc::UserListError>(missing) == adc::UserListError::inconsistent_source, "Missing metadata silently omitted a public user");
    const auto stale = fixture.list.prepare(id(6),joining,[&](users::SessionId) { return fixture.info(id(1)); },bytes);
    require(std::get<adc::UserListError>(stale) == adc::UserListError::inconsistent_source, "Wrong peer metadata accepted");
    require(fixture.sessions.snapshot(id(6)) == before && !fixture.sessions.by_sid(joining.sid()), "Planning registered/mutated the joiner");
    require(std::get<adc::UserListError>(fixture.list.prepare(id(6),*fixture.clients.at(1),{},bytes)) ==
        adc::UserListError::invalid_identity, "Already reserved own identity accepted");
    require(!fixture.clients.at(1)->observe(adc::Direction::from_client,parse("BINF " + std::string(fixture.clients.at(1)->sid()) + " NIRenamed")), "Fixture rename failed");
    require(plan.frames()[1].wire.find("NIUser1") != std::string::npos &&
        std::get<std::string>(adc::serialize(plan.frames()[1].message)).find("NIUser1") != std::string::npos, "Prepared plan borrowed mutable source strings");
    require(std::get<adc::UserListError>(fixture.prepare(6)) == adc::UserListError::inconsistent_source, "Stale registry name accepted");
    require(std::get<adc::UserListError>(fixture.prepare(1)) == adc::UserListError::invalid_state, "Already registered user restarted its list");
}
void pagination() {
    Fixture fixture;
    for (std::uint64_t number = 1; number <= 260; ++number) { fixture.add(number); }
    auto& joining = fixture.add(261,false);
    const auto result = fixture.prepare(261);
    const auto& plan = std::get<adc::InitialUserList>(result);
    require(plan.frames().size() == 262 && plan.frames().back().message.source == joining.sid(), "User list truncated at a page boundary");
    for (std::size_t index = 1; index <= 260; ++index) {
        require(plan.frames()[index].message.source == fixture.clients.at(index)->sid(), "User order changed across membership pages");
        require(plan.frames()[index].wire == std::get<std::string>(adc::serialize(plan.frames()[index].message)), "Paged welcome encoding drifted");
    }
}
void allocation_failures() {
    Fixture fixture; fixture.add(1); fixture.add(2); fixture.add(3,false);
    const auto before = *fixture.sessions.snapshot(id(3));
    const adc::UserList::InfoLookup lookup = [&](users::SessionId owner) { return fixture.info(owner); };
    bool success = false;
    for (long budget = 0; budget < 512 && !success; ++budget) {
        allocation_budget = budget;
        try { const auto result = fixture.list.prepare(id(3),*fixture.clients.at(3),lookup,1024*1024); allocation_budget = -1;
            require(std::holds_alternative<adc::InitialUserList>(result), "Allocation campaign returned an unexpected typed error"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1; }
        require(fixture.sessions.snapshot(id(3)) == before && fixture.sessions.identified_users() == 2 &&
            fixture.sessions.by_nickname("User1")->id() == id(1) && !fixture.sessions.by_sid(fixture.clients.at(3)->sid()),
            "Preparation allocation changed a model or public reservation");
    }
    require(success, "List preparation allocation campaign never completed");
}
}
int main() {
    try { contract(); pagination(); allocation_failures();
        std::cout << "PASS: bounded ordered public user synchronization, lifecycle filtering, snapshots and allocation rollback\n";
    } catch (const std::exception& error) { allocation_budget = -1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
