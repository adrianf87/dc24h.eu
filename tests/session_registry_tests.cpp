#include "dc24h/users/session_manager.hpp"

#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace dc24h::users;
using namespace std::chrono_literals;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
SessionId id(std::uint64_t value) { return *SessionId::from_value(value); }
User user(std::string_view sid, std::string_view cid, std::string_view nick, Profile profile={}) {
    return *User::create(*Identity::create(sid,cid,nick),std::move(profile));
}
template<class Error>
bool error(MutationResult result, Error expected) {
    return result && std::get_if<Error>(&*result) && std::get<Error>(*result)==expected;
}
const Session& add(SessionManager& manager, std::uint64_t value) {
    return *std::get<const Session*>(manager.add(id(value),Transport::adc,{}));
}
void identify(SessionManager& manager, std::uint64_t value, User model) {
    require(!manager.start_negotiation(id(value),{}),"Negotiation failed");
    require(!manager.identify(id(value),std::move(model),TimePoint{}+1ms),"Admission failed");
}
void invariants() {
    SessionManager manager(8);
    static_assert(std::is_same_v<decltype(manager.find(id(1))),const Session*>);
    static_assert(std::is_same_v<Registration,std::variant<const Session*,RegistryError>>);
    const auto& first=add(manager,1);
    const auto& second=add(manager,2);
    require(error(manager.mark_ready(id(99),{}),RegistryError::missing) &&
        error(manager.start_negotiation(id(99),{}),RegistryError::missing) &&
        error(manager.begin_close(id(99),CloseCause::shutdown,{}),RegistryError::missing) &&
        error(manager.force_close(id(99),CloseCause::shutdown,{}),RegistryError::missing) &&
        error(manager.finish_close(id(99),CloseCause::shutdown,{}),RegistryError::missing) &&
        error(manager.rename(id(99),"Name",{}),RegistryError::missing) &&
        error(manager.identify(id(99),user("S","C","N"),{}),RegistryError::missing) &&
        error(manager.update_user(id(99),user("S","C","N"),{}),RegistryError::missing),"Missing mutation did not return a typed error");
    require(error(manager.identify(id(1),user("SID1","CID1","First"),{}),SessionError::invalid_state) &&
        !manager.by_sid("SID1") && manager.identified_users()==0,"Invalid phase reserved identity");
    identify(manager,1,user("SID1","CID1","First"));
    require(manager.by_sid("SID1")==&first && manager.by_cid("CID1")==&first && manager.by_nickname("First")==&first,"Index does not resolve one model");
    require(!manager.start_negotiation(id(2),{}),"Second negotiation failed");
    const auto before=*manager.snapshot(id(2));
    const auto rejected=[&](User candidate, RegistryError expected) {
        require(error(manager.identify(id(2),std::move(candidate),TimePoint{}+1ms),expected) &&
            manager.snapshot(id(2))==before && manager.identified_users()==1 &&
            !manager.by_sid("SID2") && !manager.by_cid("CID2") && !manager.by_nickname("Second"),"Duplicate admission partially reserved keys");
    };
    rejected(user("SID1","CID2","Second"),RegistryError::duplicate_sid);
    rejected(user("SID2","CID1","Second"),RegistryError::duplicate_cid);
    rejected(user("SID2","CID2","First"),RegistryError::duplicate_nickname);
    require(error(manager.identify(id(2),user("SID2","CID2","Second"),TimePoint{}-1ns),SessionError::backwards_time) &&
        manager.snapshot(id(2))==before && !manager.by_sid("SID2"),"Backwards admission reserved keys");
    require(!manager.identify(id(2),user("SID2","CID2","Second"),TimePoint{}+1ms),"Second admission failed");
    const auto first_before=*manager.snapshot(id(1));
    require(error(manager.rename(id(1),"Second",TimePoint{}+2ms),RegistryError::duplicate_nickname) &&
        manager.snapshot(id(1))==first_before && manager.by_nickname("First")==&first && manager.by_nickname("Second")==&second,"Rename collision corrupted owner");
    require(error(manager.rename(id(1),"New",{}),SessionError::backwards_time) && !manager.by_nickname("New"),"Backwards rename reserved key");
    require(error(manager.rename(id(1),std::string_view("N\0X",3),TimePoint{}+2ms),SessionError::invalid_identity),"NUL rename accepted");
    require(error(manager.update_user(id(1),user("Other","CID1","First"),TimePoint{}+2ms),SessionError::invalid_identity) &&
        error(manager.update_user(id(1),user("SID1","Other","First"),TimePoint{}+2ms),SessionError::invalid_identity) &&
        manager.snapshot(id(1))==first_before,"Identity key rebound");
    ProfileData data;data.description="Updated";data.shared_files=0;
    require(!manager.update_user(id(1),user("SID1","CID1","Żółw",*Profile::create(data)),TimePoint{}+2ms),"User update failed");
    require(!manager.by_nickname("First") && manager.by_nickname("Żółw")==&first &&
        manager.by_sid("SID1")==&first && manager.by_cid("CID1")==&first && first.user()->profile().data()==data &&
        first_before.identity->nickname()=="First","Atomic identity/profile update failed");
    require(!manager.rename(id(1),"Żółw",TimePoint{}+2ms) && first.user()->profile().data()==data && manager.identified_users()==2,"No-op rename corrupted profile/index");
    require(!manager.mark_ready(id(1),TimePoint{}+3ms) && !manager.begin_close(id(1),CloseCause::idle_timeout,TimePoint{}+4ms),"Ready/close failed");
    require(manager.by_nickname("Żółw")==&first && !manager.by_nickname("Żółw")->ready() && manager.identified_users()==2,"Closing released reservation early");
    require(error(manager.rename(id(1),"Late",TimePoint{}+5ms),SessionError::invalid_state) && !manager.by_nickname("Late"),"Closing rename changed indexes");
    require(!manager.force_close(id(1),CloseCause::output_limit,TimePoint{}+5ms) && first.timeline().closing==TimePoint{}+4ms,"Force-close restarted deadline");
    require(!manager.finish_close(id(1),CloseCause::peer_closed,TimePoint{}+6ms) && manager.by_sid("SID1")==&first,"Finalization released keys before removal");
    require(!manager.remove(id(1)) && !manager.by_sid("SID1") && !manager.by_cid("CID1") && !manager.by_nickname("Żółw") && manager.identified_users()==1,"Removal leaked identity indexes");
    require(manager.by_sid("SID2")==&second && manager.by_nickname("Second")==&second,"Removing one owner damaged another");
}
void reuse_and_bounds() {
    SessionManager manager(4);
    (void)add(manager,1);identify(manager,1,user("S","C","Name"));
    const auto saved=*manager.snapshot(id(1));
    require(!manager.finish_close(id(1),CloseCause::shutdown,TimePoint{}+2ms),"Finalization failed");
    (void)add(manager,2);
    require(!manager.start_negotiation(id(2),{}),"Reconnect negotiation");
    require(error(manager.identify(id(2),user("S","C","Name"),TimePoint{}+2ms),RegistryError::duplicate_sid),"Closed reservation reused before removal");
    require(!manager.remove(id(1)) && !manager.identify(id(2),user("S","C","Name"),TimePoint{}+3ms),"Public identity not reusable after removal");
    require(!manager.find(id(1)) && manager.by_sid("S")->id()==id(2) && saved.id==id(1),"Stale local ID resolved replacement");
    (void)add(manager,3);identify(manager,3,user("s","c","name"));
    require(manager.by_nickname("Name")->id()==id(2) && manager.by_nickname("name")->id()==id(3),"Unexpected case folding");
    (void)add(manager,4);
    const std::string sid(max_session_key_bytes,'S'),cid(max_client_key_bytes,'C'),nick(max_nickname_bytes,'N');
    identify(manager,4,user(sid,cid,nick));
    require(manager.by_sid(sid)->id()==id(4) && manager.by_cid(cid)->id()==id(4) && manager.by_nickname(nick)->id()==id(4),"Exact query bounds rejected");
    require(!manager.by_sid(sid+'S') && !manager.by_cid(cid+'C') && !manager.by_nickname(nick+'N') &&
        !manager.by_sid("") && !manager.by_cid(std::string_view("C\0",2)),"Invalid lookup key accepted");
    for (std::uint64_t value=2;value<=4;++value) {
        require(!manager.finish_close(id(value),CloseCause::shutdown,TimePoint{}+4ms) && !manager.remove(id(value)),"Bounded cleanup failed");
    }
    require(manager.identified_users()==0 && manager.page().empty(),"Indexes outlived all models");
    for (std::uint64_t value=5;value<1005;++value) {
        (void)add(manager,value);identify(manager,value,user("S","C","Name"));
        require(manager.by_cid("C")->id()==id(value),"Churn lookup stale");
        require(!manager.finish_close(id(value),CloseCause::peer_closed,TimePoint{}+2ms) && !manager.remove(id(value)),"Churn cleanup failed");
        require(manager.identified_users()==0 && !manager.by_nickname("Name"),"Churn leaked reservation");
    }
}
}
int main() {
    try { invariants();reuse_and_bounds();std::cout<<"PASS: centralized identity ownership, uniqueness, rollback, cleanup and bounded lookup\n"; }
    catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
