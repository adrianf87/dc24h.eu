#include "dc24h/users/session.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace dc24h::users;
using namespace std::chrono_literals;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
Identity identity() { return *Identity::create("AAAB","PUBLIC-CID","Żółw"); }
SessionId id(std::uint64_t value) { return *SessionId::from_value(value); }
void identifiers() {
    static_assert(!std::is_copy_constructible_v<Session> && !std::is_move_constructible_v<Session>);
    static_assert(!std::is_copy_constructible_v<SessionIds> && !std::is_default_constructible_v<SessionId>);
    require(!SessionId::from_value(0),"Zero ID accepted");
    SessionIds ids(17);
    for (std::uint64_t value=17;value<1000;++value) { require(ids.acquire()->value()==value,"ID sequence reused/skipped"); }
    SessionIds last(std::numeric_limits<std::uint64_t>::max()-1);
    require(last.acquire()->value()==std::numeric_limits<std::uint64_t>::max()-1 &&
            last.acquire()->value()==std::numeric_limits<std::uint64_t>::max() && !last.acquire() && !last.acquire(),"ID exhaustion wrapped");
    bool failed=false;try { SessionIds zero(0); } catch (const std::invalid_argument&) { failed=true; }
    require(failed,"Invalid sequence start");
    require(!Identity::create("","C","N") && !Identity::create("S","","N") && !Identity::create("S","C",""),"Empty public identity");
    require(Identity::create(std::string(64,'S'),std::string(128,'C'),std::string(256,'N')).has_value(),"Exact identity storage bounds");
    require(!Identity::create(std::string(65,'S'),"C","N") && !Identity::create("S",std::string(129,'C'),"N") &&
            !Identity::create("S","C",std::string(257,'N')) && !Identity::create("S","C",std::string("a\0b",3)),"Identity storage bound/NUL bypass");
}
void lifecycle() {
    const TimePoint start{};
    Session session(id(9),Transport::adcs,start);
    const auto accepted=session.snapshot();
    require(accepted.phase==Phase::accepted && !accepted.identity && !session.was_ready(),"Accepted state");
    require(session.identify(identity(),start)==SessionError::invalid_state && session.snapshot()==accepted,"Identity before transport accepted");
    require(session.start_negotiation(start-1ns)==SessionError::backwards_time && session.snapshot()==accepted,"Backwards accepted time");
    require(!session.start_negotiation(start+1ms),"Transport ready failed");
    const auto negotiating=session.snapshot();
    require(session.mark_ready(start+2ms)==SessionError::invalid_state && session.snapshot()==negotiating,"Readiness skipped identity");
    require(!session.identify(identity(),start+2ms),"Identification failed");
    const auto identified=session.snapshot();
    require(session.identify(identity(),start+3ms)==SessionError::invalid_state && session.snapshot()==identified,"Identity rebound");
    require(!session.mark_ready(start+3ms) && session.was_ready() && session.ready(),"Ready not recorded");
    const auto ready=session.snapshot();
    require(!session.rename("Renamed",start+4ms),"Nickname update failed");
    require(ready.identity->nickname()=="Żółw" && session.identity()->nickname()=="Renamed" &&
            session.identity()->sid()==ready.identity->sid() && session.identity()->cid()==ready.identity->cid(),"Snapshot ownership/immutable identity keys");
    const auto renamed=session.snapshot();
    require(session.rename("",start+5ms)==SessionError::invalid_identity && session.snapshot()==renamed,"Invalid nickname mutated model");
    require(session.begin_close(CloseCause::idle_timeout,start+3ms)==SessionError::backwards_time && session.snapshot()==renamed,"Backwards close changed model");
    require(!session.begin_close(CloseCause::idle_timeout,start+5ms),"Closing failed");
    const auto closing=session.snapshot();
    require(!session.ready() && session.was_ready() && !session.immediate(),"Historical readiness lost");
    require(!session.begin_close(CloseCause::shutdown,start+6ms) && session.snapshot()==closing,"Repeated close moved deadline/reason");
    require(session.rename("Late",start+6ms)==SessionError::invalid_state && session.snapshot()==closing,"Closing identity mutated");
    require(!session.force_close(CloseCause::output_limit,start+7ms) && session.immediate() &&
            session.timeline().closing==start+5ms && session.terminal_cause()==CloseCause::output_limit,"Escalation reset deadline");
    require(!session.finish_close(CloseCause::close_timeout,start+8ms),"Final cleanup failed");
    const auto closed=session.snapshot();
    require(closed.phase==Phase::closed && closed.close_cause==CloseCause::idle_timeout &&
            closed.forced_cause==CloseCause::output_limit && closed.final_cause==CloseCause::close_timeout &&
            closed.timeline.closed==start+8ms && closed.timeline.ready==start+3ms,"Closure cause/timeline conflated");
    require(!session.finish_close(CloseCause::shutdown,start+9ms) && !session.force_close(CloseCause::write_error,start+9ms) &&
            !session.begin_close(CloseCause::peer_closed,start+9ms) && session.snapshot()==closed,"Closed model resurrected/mutated");
    require(session.start_negotiation(start+10ms)==SessionError::invalid_state && session.mark_ready(start+10ms)==SessionError::invalid_state,
            "Terminal phase accepted progression");
}
void early_cleanup_and_time() {
    const TimePoint start{};
    for (int phase=0;phase<4;++phase) {
        Session value(id(static_cast<std::uint64_t>(phase+1)),Transport::adc,start);
        if (phase>=1) { require(!value.start_negotiation(start+1ms),"Start fixture"); }
        if (phase>=2) { require(!value.identify(identity(),start+2ms),"Identify fixture"); }
        if (phase>=3) { require(!value.mark_ready(start+3ms),"Ready fixture"); }
        const auto before=value.snapshot();
        require(value.finish_close(CloseCause::peer_closed,start-1ms)==SessionError::backwards_time && value.snapshot()==before,"Failed cleanup mutated model");
        require(!value.finish_close(CloseCause::peer_closed,start+4ms) && value.phase()==Phase::closed && value.immediate(),"Direct EOF cleanup");
        require(value.was_ready()==(phase==3) && value.timeline().closing==start+4ms && value.timeline().closed==start+4ms,"Early readiness/timeline fabricated");
    }
}
}
int main() {
    try { identifiers();lifecycle();early_cleanup_and_time();std::cout<<"PASS: Users session IDs, identity, lifecycle, snapshots, causes and monotonic timeline\n"; }
    catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
