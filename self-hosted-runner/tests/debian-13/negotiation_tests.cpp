#include "dc24h/security/limits.hpp"
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace dc24h::security;
using namespace std::chrono_literals;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
void budgets() {
    NegotiationBudget bytes(10,3);
    require(bytes.admit_bytes(0)&&bytes.admit_bytes(4)&&bytes.admit_bytes(6)&&bytes.bytes()==10,"Exact byte cap");
    require(!bytes.admit_bytes(1)&&bytes.failed()&&!bytes.admit_bytes(0)&&!bytes.admit_frame(),"Byte failure not sticky");
    NegotiationBudget frames(10,2);
    require(frames.admit_frame()&&frames.admit_frame()&&frames.frames()==2,"Exact frame cap");
    require(!frames.admit_frame()&&!frames.admit_bytes(1),"Frame failure not sticky");
    NegotiationBudget overflow(std::numeric_limits<std::size_t>::max(),1);
    require(overflow.admit_bytes(std::numeric_limits<std::size_t>::max())&&!overflow.admit_bytes(1),"Byte counter overflow");
    NegotiationBudget fragmented(65536,64);
    for (int i=0;i<65536;++i) { require(fragmented.admit_bytes(1),"Fragmentation changes byte budget"); }
    for (int i=0;i<64;++i) { require(fragmented.admit_frame(),"Frame cap before boundary"); }
    require(!fragmented.admit_bytes(1)&&fragmented.bytes()==65536,"Cumulative budget reset");
}
void pending() {
    Limits limits;limits.max_pending_logins=2;limits.max_pending_per_ip=1;limits.max_connections_per_ip=2;
    PeerLimiter peers(limits);const Clock::time_point start{};
    require(peers.admit("4:A",start)&&!peers.admit("4:A",start),"Per-IP pending bypass");
    require(peers.admit("6:B",start)&&!peers.admit("4:C",start),"Global pending bypass");
    require(peers.pending_logins()==2&&peers.tracked_peers()==2,"Denied global admission allocated state");
    peers.complete_login("4:A");
    require(peers.pending_logins()==1&&peers.admit("4:A",start),"Completed login retained pending slot");
    require(!peers.admit("4:A",start),"Active limit lost after ready");
    peers.release("4:A",false);require(peers.pending_logins()==2,"Ready release consumed another pending slot");
    peers.release("4:A");peers.release("6:B");peers.release("missing");peers.complete_login("missing");
    require(peers.pending_logins()==0,"Pending slot leaked");
    require(peers.admit("4:C",start),"Released pending capacity unusable");peers.release("4:C");
}
void identity_windows() {
    Limits limits;limits.max_identity_checks_per_second=1;limits.max_identity_checks_per_minute=1;
    PeerLimiter peers(limits);const Clock::time_point start{};
    require(!peers.identity_check("4:A",start)&&peers.tracked_peers()==0,"Unadmitted check allocated state");
    require(peers.admit("4:A",start)&&peers.admit("4:B",start),"Fixture admission");
    require(peers.identity_check("4:A",start)&&!peers.identity_check("4:B",start+999ms),"Hub second quota");
    require(peers.identity_check("4:B",start+1s),"Denied global check charged per-IP budget/reset early");
    require(!peers.identity_check("4:A",start+59999ms),"IP minute reset early");
    require(peers.identity_check("4:A",start+60s),"IP minute never resets");
    peers.complete_login("4:A");require(!peers.identity_check("4:A",start+60s),"Ready-only IP accepted initial identity work");
    peers.release("4:A",false);peers.release("4:B");
    require(peers.admit("4:B",start+60s)&&!peers.identity_check("4:B",start+60s),"Reconnect reset a live identity window");
    require(peers.identity_check("4:B",start+61s),"Identity budget failed after minute");peers.release("4:B");
    // Admission-window expiry must not prune a later identity-check window.
    PeerLimiter late(limits);require(late.admit("4:Late",start),"Late fixture");
    require(late.identity_check("4:Late",start+59s),"Late identity rejected");late.release("4:Late");
    require(late.admit("4:Late",start+60s)&&!late.identity_check("4:Late",start+60s),"Pruning reset a newer identity window");
    late.release("4:Late");require(late.admit("4:Other",start+120s),"Expired identity entry not reusable");
    require(late.tracked_peers()==1,"Inactive expired identity retained");late.release("4:Other");
    PeerLimiter deferred(limits);
    require(deferred.admit("4:A",start)&&deferred.admit("4:B",start),"Deferred fixture");
    require(deferred.identity_check("4:A",start)&&!deferred.identity_check("4:B",start),"Deferred global gate");
    require(deferred.identity_check("4:B",start+59s)&&!deferred.identity_check("4:B",start+60s),
            "Global denial started the IP window before its first charged attempt");
    require(deferred.identity_check("4:B",start+119s),"Charged IP window did not expire");
    deferred.release("4:A");deferred.release("4:B");
}
}
int main() {
    try { budgets();pending();identity_windows();std::cout<<"PASS: negotiation bytes/frames, pending admission and shared identity windows\n"; }
    catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
