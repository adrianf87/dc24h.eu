#include "dc24h/users/user_registry.hpp"
#include "dc24h/users/session_manager.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>

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
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using namespace dc24h::users;
using namespace std::chrono_literals;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
SessionId id(std::uint64_t number) { return *SessionId::from_value(number); }
User user(std::string_view sid, std::string_view cid, std::string_view nickname = "Żółw", Profile profile = {}) {
    return *User::create(*Identity::create(sid, cid, nickname), std::move(profile));
}
template<class Error>
bool error(MutationResult result, Error expected) {
    return result && std::get_if<Error>(&*result) && std::get<Error>(*result) == expected;
}
void standalone_contract() {
    static_assert(!std::is_copy_constructible_v<UserRegistry> && !std::is_move_constructible_v<UserRegistry>);
    static_assert(std::is_same_v<decltype(std::declval<const UserRegistry&>().by_sid("")),std::optional<SessionId>>);
    static_assert(noexcept(std::declval<const UserRegistry&>().by_cid("")));
    for (const auto limit : {std::size_t{0},UserRegistry::max_capacity + 1}) {
        bool rejected = false;
        try { UserRegistry invalid(limit); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid registry capacity accepted");
    }
    UserRegistry maximum(UserRegistry::max_capacity);
    require(maximum.capacity() == UserRegistry::max_capacity && maximum.size() == 0, "Maximum capacity rejected");
    UserRegistry registry(3);
    require(!registry.by_sid("SID") && !registry.by_cid("CID") && !registry.contains(id(1)), "Empty registry resolved an owner");
    auto first = user("SID","CID");
    require(!registry.add(id(1), first), "First user rejected");
    require(registry.by_sid("SID") == id(1) && registry.by_cid("CID") == id(1) && registry.contains(id(1)), "SID/CID owners disagree");
    require(registry.add(id(1),user("NewSID","NewCID")) == UserRegistryError::owner_exists &&
        !registry.by_sid("NewSID") && !registry.by_cid("NewCID"), "One owner registered a second identity");
    require(registry.add(id(2),user("SID","OtherCID")) == UserRegistryError::duplicate_sid &&
        !registry.by_cid("OtherCID") && !registry.contains(id(2)), "SID collision partially registered");
    require(registry.add(id(2),user("OtherSID","CID")) == UserRegistryError::duplicate_cid &&
        !registry.by_sid("OtherSID") && registry.size() == 1, "CID collision partially registered");
    require(!registry.add(id(2), user("sid","cid")), "Unexpected case folding");
    // The SID and CID namespaces are independent, and need not resemble ADC
    // syntax in this neutral Users module. The ADC adapter validates the wire.
    const auto greatest = id(std::numeric_limits<std::uint64_t>::max());
    require(!registry.add(greatest,user("CID","SID")), "Independent key namespaces collided");
    require(registry.add(id(4),user("Fourth","FourthCID")) == UserRegistryError::capacity_reached &&
        !registry.by_sid("Fourth") && !registry.by_cid("FourthCID"), "Capacity exceeded");
    // The registry owns keys, not a reference to an input value or a profile copy.
    first = user("ChangedSID","ChangedCID","Different");
    require(registry.by_sid("SID") == id(1) && !registry.by_sid("ChangedSID"), "Input assignment rebound the registry");
    require(registry.remove(id(1)) && !registry.remove(id(1)) && !registry.remove(id(99)), "Removal is not owner specific");
    require(!registry.add(id(4),user("SID","NewCID")) && !registry.remove(id(1)) && registry.by_sid("SID") == id(4),
        "Stale cleanup removed reused SID");
    require(registry.by_cid("cid") == id(2) && registry.by_sid("CID") == greatest, "Removing an owner damaged another entry");
    require(registry.remove(id(4)) && !registry.add(id(5),user("NewSID","CID")) && registry.by_cid("CID") == id(5), "CID not reusable after removal");
    require(registry.remove(id(2)) && registry.remove(greatest) && registry.remove(id(5)) && registry.size() == 0,
        "Cleanup leaked indexes");
    const std::string sid(max_session_key_bytes,'S'), cid(max_client_key_bytes,'C');
    require(!registry.add(id(6),user(sid,cid)), "Exact generic key bounds rejected");
    const std::string oversize_sid = sid + 'S', oversize_cid = cid + 'C';
    allocation_budget = 0;
    const bool queries = registry.by_sid(sid) == id(6) && registry.by_cid(cid) == id(6) &&
        !registry.by_sid(oversize_sid) && !registry.by_cid(oversize_cid) && !registry.by_sid("") &&
        !registry.by_cid(std::string_view("C\0",2)) && registry.contains(id(6)) && registry.remove(id(6));
    allocation_budget = -1;
    require(queries && registry.size() == 0, "Lookup/removal allocated or violated query bounds");
    for (std::uint64_t number = 7; number < 1007; ++number) {
        require(!registry.add(id(number),user("S","C")) && registry.by_sid("S") == id(number) &&
            registry.by_cid("C") == id(number) && registry.remove(id(number)) && registry.size() == 0,
            "Repeated lifetime cleanup failed");
    }
}
void managed_users() {
    SessionManager manager(3);
    static_assert(std::is_same_v<decltype(manager.user_by_sid("")),const User*>);
    static_assert(std::is_same_v<decltype(manager.user_by_cid("")),const User*>);
    (void)manager.add(id(1),Transport::adc,{});
    require(!manager.by_sid("S") && !manager.user_by_cid("C") && manager.identified_users() == 0, "Pending session registered a user");
    require(!manager.start_negotiation(id(1),{}), "Negotiation failed");
    require(!manager.identify(id(1),user("S","C"),TimePoint{}+1ms), "Admission failed");
    const auto* session = manager.find(id(1));
    require(manager.user_by_sid("S") == &*session->user() && manager.user_by_cid("C") == &*session->user(),
        "User lookup returned a copied or different model");
    const auto saved = *manager.snapshot(id(1));
    ProfileData data; data.description = "Updated"; data.slots = 3;
    require(!manager.update_user(id(1),user("S","C","Łoś",*Profile::create(data)),TimePoint{}+2ms), "User update failed");
    require(manager.user_by_sid("S")->identity().nickname() == "Łoś" && manager.user_by_cid("C")->profile().data() == data &&
        saved.identity->nickname() == "Żółw" && manager.identified_users() == 1, "Registry exposed stale identity/profile");
    const auto updated = *manager.snapshot(id(1));
    require(error(manager.update_user(id(1),user("New","C"),TimePoint{}+3ms),SessionError::invalid_identity) &&
        error(manager.update_user(id(1),user("S","New"),TimePoint{}+3ms),SessionError::invalid_identity) &&
        manager.snapshot(id(1)) == updated && !manager.user_by_sid("New") && !manager.user_by_cid("New"), "Live identity was rebound");
    require(!manager.mark_ready(id(1),TimePoint{}+3ms) && !manager.begin_close(id(1),CloseCause::shutdown,TimePoint{}+4ms), "Close failed");
    require(!manager.by_sid("S")->ready() && manager.user_by_cid("C") && manager.identified_users() == 1, "Closing dropped public reservation");
    require(!manager.finish_close(id(1),CloseCause::shutdown,TimePoint{}+5ms) && manager.user_by_sid("S"), "Finalization dropped reservation early");
    require(!manager.remove(id(1)) && !manager.user_by_sid("S") && !manager.user_by_cid("C") && manager.identified_users() == 0,
        "Removal left a stale public user");
    (void)manager.add(id(2),Transport::adcs,{});
    require(!manager.start_negotiation(id(2),{}) && !manager.identify(id(2),user("S","C"),TimePoint{}+6ms) &&
        manager.by_cid("C")->id() == id(2) && !manager.find(id(1)), "Reconnect resolved the old lifetime");
}
void allocation_failures() {
    const auto candidate = user(std::string(max_session_key_bytes,'S'),std::string(max_client_key_bytes,'C'),std::string(200,'N'));
    bool success = false;
    for (long budget = 0; budget < 16 && !success; ++budget) {
        UserRegistry registry(2);
        require(!registry.add(id(1),user("SentinelSID","SentinelCID")), "Sentinel setup failed");
        allocation_budget = budget;
        try { const auto result = registry.add(id(2),candidate); allocation_budget = -1;
            require(!result, "Unexpected registry error"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1;
            require(registry.size() == 1 && !registry.contains(id(2)) && !registry.by_sid(candidate.identity().sid()) &&
                !registry.by_cid(candidate.identity().cid()), "Failed allocation left partial SID/CID/owner state");
            require(!registry.add(id(2),candidate), "Allocation failure blocked registry retry");
        }
        require(registry.by_sid("SentinelSID") == id(1) && registry.by_cid("SentinelCID") == id(1) &&
            registry.by_sid(candidate.identity().sid()) == id(2) && registry.by_cid(candidate.identity().cid()) == id(2),
            "Registry rollback/retry changed an owner");
    }
    require(success, "Registry allocation campaign never completed");
    success = false;
    for (long budget = 0; budget < 24 && !success; ++budget) {
        SessionManager manager(2);
        (void)manager.add(id(1),Transport::adc,{}); (void)manager.add(id(2),Transport::adcs,{});
        require(!manager.start_negotiation(id(1),{}) && !manager.start_negotiation(id(2),{}) &&
            !manager.identify(id(1),user("FirstSID","FirstCID","First"),TimePoint{}+1ms), "Managed setup failed");
        const auto before = *manager.snapshot(id(2));
        auto input = candidate;
        allocation_budget = budget;
        try { const auto result = manager.identify(id(2),std::move(input),TimePoint{}+2ms); allocation_budget = -1;
            require(!result, "Unexpected admission error"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1;
            require(manager.snapshot(id(2)) == before && manager.identified_users() == 1 &&
                !manager.by_sid(candidate.identity().sid()) && !manager.by_cid(candidate.identity().cid()) &&
                !manager.by_nickname(candidate.identity().nickname()), "Admission left partial registry/nickname/model state");
            require(!manager.identify(id(2),candidate,TimePoint{}+2ms), "Admission allocation failure blocked retry");
        }
        require(manager.by_sid("FirstSID")->id() == id(1) && manager.by_cid("FirstCID")->id() == id(1) &&
            manager.by_nickname("First")->id() == id(1) && manager.user_by_sid(candidate.identity().sid()) ==
            manager.user_by_cid(candidate.identity().cid()) && manager.identified_users() == 2, "Managed retry corrupted ownership");
    }
    require(success, "Admission allocation campaign never completed");
}
}
int main() {
    try { standalone_contract(); managed_users(); allocation_failures();
        std::cout << "PASS: SID/CID registry, owner uniqueness, lifecycle, bounded lookup, reuse and allocation rollback\n";
    } catch (const std::exception& error) { allocation_budget = -1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
