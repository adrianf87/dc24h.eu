#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { bool fail_allocations=false; }
void* operator new(std::size_t size) {
    if (fail_allocations) { throw std::bad_alloc(); }
    if (auto* memory=std::malloc(size==0?1:size)) { return memory; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory,std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory,std::size_t) noexcept { std::free(memory); }

namespace {
using namespace dc24h;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
users::User user(std::uint64_t value) {
    const auto suffix=std::to_string(value);
    return *users::User::create(*users::Identity::create("S"+suffix,"C"+suffix,"N"+suffix));
}
void reconcile(const users::SessionManager& manager) {
    users::SessionCounts actual{};
    for (const auto& session:manager.page()) {
        ++actual.total;
        switch (session.phase) {
        case users::Phase::accepted: ++actual.accepted;break;
        case users::Phase::negotiating: ++actual.negotiating;break;
        case users::Phase::identified: ++actual.identified;break;
        case users::Phase::ready: ++actual.ready;break;
        case users::Phase::closing: ++actual.closing;break;
        case users::Phase::closed: ++actual.closed;break;
        }
    }
    const auto stats=manager.statistics();
    require(actual==manager.counts() && actual==stats.sessions && stats.online()==actual.ready,"Cached counts differ from owned lifetimes");
    require(stats.created_sessions-stats.completed_sessions==actual.total-actual.closed,"Completed lifetime total inconsistent");
}
void lifecycle() {
    users::SessionManager manager(8);
    require(manager.statistics()==users::Statistics{},"Nonzero initial statistics");
    for (std::uint64_t owner=1;owner<=6;++owner) {
        require(std::holds_alternative<const users::Session*>(manager.add(id(owner),users::Transport::adc,{})),"Add failed");
        if (owner>=2) { require(!manager.start_negotiation(id(owner),{}),"Negotiation failed"); }
        if (owner>=3) { require(!manager.identify(id(owner),user(owner),{}),"Identification failed"); }
        if (owner>=4) { require(!manager.mark_ready(id(owner),{}),"Readiness failed"); }
        if (owner>=5) { require(!manager.begin_close(id(owner),users::CloseCause::idle_timeout,{}),"Close failed"); }
        if (owner>=6) { require(!manager.finish_close(id(owner),users::CloseCause::peer_closed,{}),"Finish failed"); }
        reconcile(manager);
    }
    const auto before=manager.statistics();
    require(before.online()==1 && before.peak_online==2 && before.successful_logins==3 && before.completed_sessions==1,
            "Online, peak or historical totals conflate lifecycle phases");
    require(manager.mark_ready(id(4),{}).has_value() && manager.mark_ready(id(99),{}).has_value(),"Invalid ready accepted");
    require(manager.begin_close(id(4),users::CloseCause::shutdown,users::TimePoint{}-std::chrono::seconds(1)).has_value(),"Backwards close accepted");
    require(!manager.begin_close(id(5),users::CloseCause::shutdown,{}) && !manager.finish_close(id(6),users::CloseCause::shutdown,{}),"Idempotent close failed");
    require(manager.statistics()==before,"Failed or repeated mutations changed statistics");
    auto candidate=user(2);
    fail_allocations=true;
    bool failed=false;
    try { (void)manager.identify(id(2),std::move(candidate),{}); } catch (const std::bad_alloc&) { failed=true; }
    fail_allocations=false;
    require(failed && manager.statistics()==before,"Failed identity allocation counted a transition");
    fail_allocations=true;
    try { (void)manager.add(id(7),users::Transport::adcs,{});failed=false; } catch (const std::bad_alloc&) { failed=true; }
    // Snapshot reads and all terminal operations must remain allocation-free.
    const auto noalloc=manager.statistics();
    for (std::uint64_t owner=1;owner<=6;++owner) {
        if (manager.finish_close(id(owner),users::CloseCause::shutdown,{}) || manager.remove(id(owner))) { std::abort(); }
    }
    const auto final=manager.statistics();
    fail_allocations=false;
    require(failed && noalloc==before && final.sessions==users::SessionCounts{} && final.completed_sessions==6 &&
            final.created_sessions==6 && final.successful_logins==3 && final.peak_online==2,"Cleanup/history or no-allocation statistics failed");
    require(std::holds_alternative<const users::Session*>(manager.add(id(7),users::Transport::adcs,{})),"Retry add failed");
    reconcile(manager);
    users::SessionManager fresh(1);require(fresh.statistics()==users::Statistics{},"Statistics leaked across instances");
}
adc::Message parse(std::string_view value) { return std::get<adc::Message>(adc::parse_line(value)); }
void welcome_boundary() {
    adc::LoginHub hub;
    for (std::uint64_t owner=1;owner<=3;++owner) {
        hub.connect(owner,"127.0.0.1");hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
        hub.consume(owner,hub.output(owner).size());
        const std::string sid(hub.output(owner).substr(5,4));hub.consume(owner,hub.output(owner).size());
        adc::TigerDigest pid{};pid[0]=static_cast<std::uint8_t>(owner);
        hub.receive(owner,parse("BINF "+sid+" NINick"+std::to_string(owner)+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)));
        while (!hub.output(owner).empty()) {
            const auto frame=hub.output(owner);
            if (frame.starts_with("BINF "+sid+" ")) { break; }
            hub.consume(owner,frame.size());
        }
        hub.consume(owner,hub.output(owner).size()-1); // Own INF LF still pending.
        require(hub.sessions().statistics().successful_logins==owner-1 && hub.sessions().statistics().online()==owner-1,
                "Partial welcome counted online");
        if (owner==3) { // Canceled welcome must never count as a login.
            hub.disconnect(owner,false);
            require(hub.sessions().statistics().successful_logins==2 && hub.sessions().statistics().online()==2,"Canceled welcome counted login");
        } else {
            require(hub.consume(owner,1),"Final welcome LF did not commit");
            require(hub.sessions().statistics().online()==owner,"Final LF missing from online count");
        }
        reconcile(hub.sessions());
    }
    hub.close(1,adc::CloseReason::idle_timeout);
    require(hub.sessions().statistics().online()==1 && hub.users()==2,"Closing still online or released identity early");
    hub.disconnect(1,false);hub.disconnect(1,false);hub.disconnect(2,false);
    const auto final=hub.sessions().statistics();
    require(final.online()==0 && final.peak_online==2 && final.successful_logins==2 && final.completed_sessions==3,
            "Disconnect/repeated cleanup lost statistics");
}
}
int main() {
    try { lifecycle();welcome_boundary();std::cout<<"PASS: lifecycle statistics, exact welcome boundary, allocation rollback and cleanup\n"; }
    catch (const std::exception& error) { fail_allocations=false;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
