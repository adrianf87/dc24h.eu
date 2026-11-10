#include "dc24h/adc/user_disconnect.hpp"
#include "dc24h/adc/user_login.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/status.hpp"
#include "dc24h/adc/serializer.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
long allocation_budget = -1;
bool fail_once = false;
void* allocate(std::size_t bytes) {
    if (allocation_budget == 0) {
        if (fail_once) { allocation_budget = -1; }
        throw std::bad_alloc();
    }
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
users::User user(std::string_view sid="AAAB") { return *users::User::create(*users::Identity::create(sid,"PublicCID","Name")); }
void lifecycle() {
    for (const auto phase : {users::Phase::accepted,users::Phase::negotiating,users::Phase::identified,
            users::Phase::ready,users::Phase::closing,users::Phase::closed}) {
        users::SessionManager sessions(2); adc::UserDisconnect disconnect(sessions);
        (void)sessions.add(id(1),users::Transport::adcs,{});
        if (phase>=users::Phase::negotiating) { require(!sessions.start_negotiation(id(1),{}),"Negotiation failed"); }
        if (phase>=users::Phase::identified) { require(!sessions.identify(id(1),user(),users::TimePoint{}+1ms),"Identify failed"); }
        if (phase>=users::Phase::ready) { require(!sessions.mark_ready(id(1),users::TimePoint{}+2ms),"Ready failed"); }
        if (phase>=users::Phase::closing) { require(!sessions.begin_close(id(1),users::CloseCause::idle_timeout,users::TimePoint{}+3ms),"Close failed"); }
        if (phase==users::Phase::closed) { require(!sessions.finish_close(id(1),users::CloseCause::peer_closed,users::TimePoint{}+4ms),"Finish failed"); }
        const auto before=*sessions.snapshot(id(1));
        if (phase!=users::Phase::closed) { require(disconnect.release(id(1))==users::RegistryError::not_closed,"Live model released before finalization"); }
        allocation_budget=0;
        const auto result=disconnect.begin(id(1),users::CloseCause::peer_closed,users::TimePoint{}+5ms);
        allocation_budget=-1;
        require(std::holds_alternative<adc::Departure>(result),"Finalization failed");
        const auto departure=std::get<adc::Departure>(result);
        require(departure.first==(phase!=users::Phase::closed) && departure.was_ready==(phase>=users::Phase::ready) &&
            departure.public_sid.has_value()==(phase>=users::Phase::identified && phase!=users::Phase::closed) &&
            !departure.invalid_public_sid && sessions.find(id(1))->phase()==users::Phase::closed,"Departure phase/publication history wrong");
        if (phase>=users::Phase::identified) { require(sessions.by_cid("PublicCID") && sessions.by_nickname("Name"),"Finalization released reservations before owner cleanup"); }
        const auto closed=*sessions.snapshot(id(1));
        const auto again=std::get<adc::Departure>(disconnect.begin(id(1),users::CloseCause::shutdown,{}));
        require(!again.first && !again.public_sid && sessions.snapshot(id(1))==closed,"Repeated begin changed history or duplicated public QUI");
        if (phase>=users::Phase::closing) { require(closed.close_cause==before.close_cause && closed.timeline.closing==before.timeline.closing,"Finalization replaced first close cause/deadline"); }
        allocation_budget=0; const auto released=disconnect.release(id(1)); allocation_budget=-1;
        require(!released && !sessions.find(id(1)) && !sessions.by_sid("AAAB") && !sessions.by_cid("PublicCID") &&
            !sessions.by_nickname("Name") && sessions.size()==0,"Release allocated or leaked registry keys");
        require(disconnect.release(id(1))==users::RegistryError::missing &&
            std::get<users::RegistryError>(disconnect.begin(id(1),users::CloseCause::shutdown,{}))==users::RegistryError::missing,"Missing cleanup not typed");
    }
    users::SessionManager sessions(1);adc::UserDisconnect disconnect(sessions);
    (void)sessions.add(id(1),users::Transport::adc,{});(void)sessions.start_negotiation(id(1),{});
    require(!sessions.identify(id(1),user("OpaqueGenericSID"),users::TimePoint{}+1ms),"Generic fixture failed");
    const auto before=*sessions.snapshot(id(1));
    require(std::get<users::SessionError>(disconnect.begin(id(1),users::CloseCause::shutdown,{}))==users::SessionError::backwards_time &&
        sessions.snapshot(id(1))==before,"Backwards finalization changed the model");
    const auto invalid=std::get<adc::Departure>(disconnect.begin(id(1),users::CloseCause::shutdown,users::TimePoint{}+2ms));
    require(invalid.invalid_public_sid && !invalid.public_sid && !disconnect.release(id(1)),"Invalid ADC SID prevented cleanup");
}
adc::Message identity(std::string_view sid,unsigned seed,std::string_view nickname) {
    adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(seed);
    return parse("BINF "+std::string(sid)+" NI"+std::string(nickname)+" ID"+
        adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid));
}
void sid_lease_reuse() {
    users::SessionManager sessions(2);adc::UserLogin login(sessions);adc::UserDisconnect disconnect(sessions);
    const auto pool=std::make_shared<adc::SidPool>(1);
    for (std::uint64_t number=1;number<=2;++number) {
        (void)sessions.add(id(number),users::Transport::adc,{});require(!sessions.start_negotiation(id(number),{}),"Lease negotiation failed");
        adc::ClientSession client(pool,"127.0.0.1");
        require(!client.observe(adc::Direction::from_client,parse("HSUP ADBASE ADTIGR")) &&
            !client.observe(adc::Direction::to_client,*client.support().initial_reply()) && !client.prepare_sid() &&
            !client.observe(adc::Direction::to_client,*client.sid_reply()) &&
            !client.observe(adc::Direction::from_client,identity(client.sid(),1,"Reuse")) &&
            !client.approve_identity(adc::IdentityMode::guest) && !login.register_guest(id(number),client,users::TimePoint{}+1ms),"One-SID admission failed");
        require(pool->size()==1 && client.sid()=="AAAB","SID lease not held/reused");
        const auto departure=std::get<adc::Departure>(disconnect.begin(id(number),users::CloseCause::peer_closed,users::TimePoint{}+2ms));
        allocation_budget=0;client.disconnect();const auto released=disconnect.release(id(number));allocation_budget=-1;
        require(!released && pool->size()==0 && sessions.size()==0 && departure.public_sid==adc::Sid::parse("AAAB"),"Protocol lease or registry cleanup leaked");
        require(disconnect.release(id(number))==users::RegistryError::missing,"Old lifetime release was not isolated");
    }
}
std::vector<std::string> drain(adc::LoginHub& hub,std::uint64_t owner) {
    std::vector<std::string> output;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) {
        output.emplace_back(hub.output(owner));hub.consume(owner,output.back().size());
    }
    return output;
}
std::string connect(adc::LoginHub& hub,std::uint64_t owner,bool complete=true) {
    hub.connect(owner,"127.0.0.1");hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size());const std::string sid(hub.output(owner).substr(5,4));hub.consume(owner,hub.output(owner).size());
    hub.receive(owner,identity(sid,static_cast<unsigned>(owner),"User"+std::to_string(owner)));
    if (complete) { (void)drain(hub,owner); }
    return sid;
}
void queue_reclamation_and_order() {
    const auto status=std::get<std::string>(adc::serialize(adc::make_status(*adc::StatusCode::parse("125"),"Command not implemented","BMSG")));
    adc::LoginHub hub({4,256,status.size()*6+8});
    const auto first=connect(hub,1),second=connect(hub,2);drain(hub,1);
    for (unsigned count=0;count<3;++count) {
        hub.receive(1,parse("BMSG "+first+" Probe"));hub.receive(2,parse("BMSG "+second+" Probe"));
    }
    require(hub.queued_bytes()==status.size()*6,"Queue fixture accounting wrong");
    const auto pinned=hub.output(2);const auto* pointer=pinned.data();
    const auto saved=*hub.snapshot(1);hub.disconnect(1);
    require(!hub.snapshot(1) && !hub.sessions().by_sid(first) && !hub.sessions().by_cid(saved.identity->cid()) &&
        !hub.sessions().by_nickname("User1") && hub.ready(2) && hub.output(2).data()==pointer && hub.output(2)==pinned,
        "Departure failed to reclaim capacity before QUI or changed pinned output");
    const auto after=hub.queued_bytes();hub.disconnect(1);require(hub.queued_bytes()==after,"Repeated disconnect duplicated QUI");
    const auto output=drain(hub,2);
    require(output==std::vector<std::string>{status,status,status,"IQUI "+first+"\n"},"Public QUI order/reason isolation failed");
    hub.disconnect(2,false);require(hub.users()==0 && hub.queued_bytes()==0,"Final queues/models leaked");

    adc::LoginHub pending;const auto observer=connect(pending,1);(void)observer;
    const auto unfinished=connect(pending,2,false);drain(pending,1);
    const auto own=pending.output(2);pending.consume(2,own.size()-1); // Partial hub INF, not a completed welcome.
    pending.disconnect(2);require(drain(pending,1)==std::vector<std::string>{"IQUI "+unfinished+"\n"} &&
        !pending.snapshot(2) && pending.users()==1,"Identified but unready departure was not cleaned/announced");
    pending.connect(3,"127.0.0.1",users::Transport::adcs);pending.disconnect(3);
    require(pending.output(1).empty(),"Anonymous TLS connection emitted public QUI");pending.disconnect(1,false);
}
void allocation_failures() {
    bool success=false,isolated=false;
    for (long budget=0;budget<64 && !success;++budget) {
        adc::LoginHub hub;const auto first=connect(hub,1),second=connect(hub,2);drain(hub,1);
        hub.receive(2,parse("BMSG "+second+" Pinned"));
        const auto pinned=hub.output(2);const auto* pointer=pinned.data();const auto saved=*hub.snapshot(1);
        allocation_budget=budget;hub.disconnect(1);allocation_budget=-1;
        require(!hub.snapshot(1) && !hub.sessions().by_sid(first) && !hub.sessions().by_cid(saved.identity->cid()) &&
            !hub.sessions().by_nickname("User1") && hub.output(2).data()==pointer && hub.output(2)==pinned,"QUI allocation blocked cleanup or changed pinned output");
        if (hub.ready(2)) {
            const auto output=drain(hub,2);
            require(output.size()==2 && output.back()=="IQUI "+first+"\n","Surviving recipient missed/duplicated QUI");success=true;
        } else {
            require(hub.must_close(2) && hub.session_model(2).terminal_cause()==users::CloseCause::internal_error &&
                hub.sessions().by_nickname("User2"),"Failed notice recipient kept stale usable membership or lost reservations early");isolated=true;
        }
        hub.disconnect(2,false);require(hub.users()==0 && hub.queued_bytes()==0 && hub.sessions().size()==0,"Allocation cleanup leaked resources");
    }
    require(success && isolated,"QUI allocation campaign did not cover failure and success");

    // A transient per-recipient failure must not abort fanout to other peers.
    bool continued=false;
    for (long budget=0;budget<64 && !continued;++budget) {
        adc::LoginHub hub;const auto source=connect(hub,1);connect(hub,2);connect(hub,3);
        drain(hub,1);drain(hub,2);
        fail_once=true;allocation_budget=budget;hub.disconnect(1);allocation_budget=-1;fail_once=false;
        require(!hub.snapshot(1),"Transient notification failure retained departing source");
        unsigned ready=0,closed=0;
        for (const auto recipient : {2U,3U}) {
            if (hub.ready(recipient)) {
                ++ready;require(drain(hub,recipient)==std::vector<std::string>{"IQUI "+source+"\n"},"Healthy recipient missed departure after transient allocation failure");
            } else {
                ++closed;require(hub.must_close(recipient) && hub.session_model(recipient).terminal_cause()==users::CloseCause::internal_error,
                    "Failed recipient retained usable stale presence");
            }
        }
        continued=ready==1 && closed==1;
        hub.disconnect(2,false);hub.disconnect(3,false);
        require(hub.users()==0 && hub.queued_bytes()==0,"Transient failure retained resources");
    }
    require(continued,"Allocation campaign did not reach isolated per-recipient fanout failure");
}
}
int main() {
    try { lifecycle();sid_lease_reuse();queue_reclamation_and_order();allocation_failures();
        std::cout<<"PASS: allocation-free finalization, resource release, QUI order, capacity reuse and notification failure isolation\n";
    } catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
