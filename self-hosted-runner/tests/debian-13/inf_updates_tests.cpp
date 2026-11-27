#include "dc24h/adc/inf_updates.hpp"
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
std::string wire(const adc::InfUpdateResult& result) {
    require(std::holds_alternative<adc::AppliedInfUpdate>(result),"Expected applied update");
    const auto& message=std::get<adc::AppliedInfUpdate>(result).message;
    require(message.has_value(),"Expected public delta");
    return std::get<std::string>(adc::serialize(*message));
}
void applied_fields() {
    Fixture fixture;auto& client=fixture.add(1);const auto sid=std::string(client.sid());
    const auto original=*fixture.sessions.snapshot(id(1));
    const auto result=fixture.apply(" NIŻółw DEZażółć\\sgęślą SS0 SF0 SL0 I40.0.0.0 US4096 XXprivate");
    require(wire(result)=="BINF "+sid+" NIŻółw DEZażółć\\sgęślą SS0 SF0 SL0 I4127.0.0.1 US4096\n","Public delta repeated untouched fields or leaked unvalidated data");
    const auto* user=fixture.sessions.user_by_sid(sid);
    require(user && user==fixture.sessions.user_by_cid(original.identity->cid()) && user->identity().nickname()=="Żółw" &&
        user->profile().data().description=="Zażółć gęślą" && user->profile().data().shared_bytes==0 &&
        client.info()->integer("SS")==0 && client.info()->field("EM")=="old@example.test" &&
        !fixture.sessions.by_nickname("User1") && fixture.sessions.by_nickname("Żółw"),"ADC/User/nickname commit is inconsistent");
    require(original.identity->nickname()=="User1" && original.profile->data().description=="Original","Old snapshot changed");
    const auto canceled=fixture.apply(" DE EM SS SF SL SU US RF",users::TimePoint{}+4ms);
    require(wire(canceled)=="BINF "+sid+" DE EM SS SF SL SU US\n","Cancellation/absent-field filtering failed");
    const auto* canceled_user=fixture.sessions.user_by_sid(sid);
    require(!client.info()->field("DE") && !client.info()->field("US") && canceled_user->profile().data().description.empty() &&
        !canceled_user->profile().data().shared_bytes && canceled_user->profile().data().features.empty(),"Cancellation left stale metadata/profile");
    require(wire(fixture.apply(" NIŻółw",users::TimePoint{}+5ms))=="BINF "+sid+" NIŻółw\n","Explicit unchanged public field was not echoed");
    for (const auto* fields : {" XXprivate YY", ""}) {
        const auto ignored=fixture.apply(fields,users::TimePoint{}+6ms);
        require(std::holds_alternative<adc::AppliedInfUpdate>(ignored) && !std::get<adc::AppliedInfUpdate>(ignored).message,"Unknown/empty input generated public data");
    }
}
void rejection_is_atomic() {
    Fixture fixture;auto& client=fixture.add(1);fixture.add(2);
    const auto before=*client.info();const auto model=*fixture.sessions.snapshot(id(1));
    for (const auto* fields : {" NIUnused DEBad SS-1"," NIUser2 DEBad"," NI DEBad"," PDPRIVATE DEBad",
            " CT4 DEBad"," I4192.0.2.1 DEBad"," DEFirst DESecond"," SUabcD"," DEbad\\nline"}) {
        const auto result=fixture.apply(fields);
        require(std::holds_alternative<adc::InfUpdateFailure>(result) && *client.info()==before && fixture.sessions.snapshot(id(1))==model &&
            !fixture.sessions.by_nickname("Unused") && fixture.sessions.by_nickname("User2")->id()==id(2),"Rejected update partially committed ADC/User/index state");
    }
    const auto backwards=fixture.apply(" NIUnused DEBad",users::TimePoint{});
    require(std::holds_alternative<adc::InfUpdateFailure>(backwards) && *client.info()==before &&
        fixture.sessions.snapshot(id(1))==model,"Backwards update changed either model");
    auto spoof=parse("BINF "+std::string(fixture.clients.at(2)->sid())+" DEBad");
    require(std::holds_alternative<adc::InfUpdateFailure>(fixture.updates.apply(id(1),client,spoof,users::TimePoint{}+3ms)) &&
        *client.info()==before,"Source spoof mutated metadata");
    require(std::holds_alternative<adc::InfUpdateFailure>(fixture.updates.apply(id(99),client,spoof,{})),"Missing session accepted");
    require(!fixture.sessions.begin_close(id(1),users::CloseCause::shutdown,users::TimePoint{}+3ms),"Close failed");
    require(std::holds_alternative<adc::InfUpdateFailure>(fixture.apply(" DEAfterClose",users::TimePoint{}+4ms)) && *client.info()==before,"Closing user updated");
    Fixture pending;auto& waiting=pending.add(1,false);const auto waiting_info=*waiting.info();
    require(std::holds_alternative<adc::InfUpdateFailure>(pending.apply(" DEEarly")) && *waiting.info()==waiting_info,"Welcome-pending user updated");
    Fixture stale;auto& unsynced=stale.add(1);
    require(!unsynced.observe(adc::Direction::from_client,parse("BINF "+std::string(unsynced.sid())+" DEOutside")),"Stale fixture failed");
    const auto failure=std::get<adc::InfUpdateFailure>(stale.apply(" DEOverwrite"));
    require(std::get<adc::InfUpdateError>(failure)==adc::InfUpdateError::identity_mismatch,"Unsynchronized adapter model was silently overwritten");
}
void allocation_failures() {
    Fixture fixture;auto& client=fixture.add(1);fixture.add(2);
    const auto before=*client.info();const auto model=*fixture.sessions.snapshot(id(1));
    const std::string nickname(50,'N');
    const auto input=parse("BINF "+std::string(client.sid())+" NI"+nickname+" DE"+std::string(150,'D')+" EM SS0");
    bool success=false;
    for (long budget=0;budget<512 && !success;++budget) {
        allocation_budget=budget;
        try { const auto result=fixture.updates.apply(id(1),client,input,users::TimePoint{}+3ms);allocation_budget=-1;
            require(std::holds_alternative<adc::AppliedInfUpdate>(result),"Unexpected allocation campaign error");success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1;
            require(*client.info()==before && fixture.sessions.snapshot(id(1))==model && !fixture.sessions.by_nickname(nickname) &&
                fixture.sessions.by_nickname("User1")->id()==id(1) && fixture.sessions.by_nickname("User2")->id()==id(2),
                "Allocation failure partially committed ADC/User/nickname or affected another user");
        }
    }
    require(success && client.info()->field("NI")==nickname && fixture.sessions.user_by_sid(client.sid())->identity().nickname()==nickname &&
        !fixture.sessions.by_nickname("User1"),"Update allocation retry never committed consistently");
}
}
int main() {
    try { applied_fields();rejection_is_atomic();allocation_failures();
        std::cout<<"PASS: transactional INF delta/cancellation, identity gates, model consistency and allocation rollback\n";
    } catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
