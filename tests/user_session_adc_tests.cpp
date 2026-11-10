#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace dc24h;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
adc::Message parse(std::string_view line) { return std::get<adc::Message>(adc::parse_line(line)); }
unsigned drain(adc::LoginHub& hub,std::uint64_t id) {
    unsigned readiness=0;
    while (!hub.output(id).empty() && !hub.must_close(id)) {
        if (hub.consume(id,1)) { ++readiness; }
    }
    return readiness;
}
std::string identify(adc::LoginHub& hub,std::uint64_t id) {
    hub.receive(id,parse("HSUP ADBASE ADTIGR"));
    require(!hub.consume(id,hub.output(id).size()),"SUP completed readiness");
    const std::string line(hub.output(id));
    require(!hub.consume(id,line.size()),"SID completed readiness");return line.substr(5,4);
}
adc::Message info(std::string_view sid) {
    adc::TigerDigest pid{};
    return parse("BINF "+std::string(sid)+" NIUser ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid));
}
void adapter() {
    adc::LoginHub hub;
    require(!hub.snapshot(1),"Missing snapshot fabricated");
    hub.connect(1,"127.0.0.1",users::Transport::adcs);
    auto accepted=*hub.snapshot(1);
    require(accepted.phase==users::Phase::accepted && accepted.transport==users::Transport::adcs,"TLS accepted phase/transport");
    bool rejected=false;try { hub.receive(1,parse("HSUP ADBASE ADTIGR")); } catch (const std::logic_error&) { rejected=true; }
    require(rejected && hub.snapshot(1)==accepted && hub.output(1).empty(),"Application input before TLS readiness");
    hub.transport_ready(1);
    require(hub.session_model(1).phase()==users::Phase::negotiating,"TLS readiness not applied");
    const auto sid=identify(hub,1);
    hub.receive(1,info(sid));
    const auto identified=*hub.snapshot(1);
    require(identified.phase==users::Phase::identified && identified.identity->sid()==sid && !identified.timeline.ready &&
            hub.state(1)==adc::ConnectionState::normal && !hub.ready(1),"Protocol NORMAL conflated with application readiness");
    require(drain(hub,1)==1 && hub.ready(1) && hub.session_model(1).was_ready(),"Exactly-once welcome signal");
    const auto ready=*hub.snapshot(1);
    hub.receive(1,parse("BINF "+sid+" NIRenamed"));require(drain(hub,1)==0,"Ordinary update duplicated readiness");
    require(hub.snapshot(1)->identity->nickname()=="Renamed" && ready.identity->nickname()=="User","Snapshot aliases live nickname");
    hub.close(1,adc::CloseReason::idle_timeout);
    const auto closing=*hub.snapshot(1);
    hub.close(1,adc::CloseReason::login_timeout);
    require(hub.snapshot(1)==closing && closing.close_cause==users::CloseCause::idle_timeout &&
            closing.timeline.ready.has_value() && !hub.ready(1),"Close deadline/reason/readiness changed");
    require(drain(hub,1)==0 && hub.must_close(1),"Terminal frames marked ready");
    hub.disconnect(1);hub.disconnect(1);
    require(!hub.snapshot(1) && hub.users()==0 && hub.queued_bytes()==0 && ready.identity->sid()==sid,"Cleanup/stale snapshot");
    rejected=false;try { hub.connect(1,"127.0.0.1"); } catch (const std::logic_error&) { rejected=true; }
    require(rejected && !hub.snapshot(1),"Old session ID reused");
    hub.connect(2,"127.0.0.1");const auto new_sid=identify(hub,2);hub.receive(2,info(new_sid));
    require(drain(hub,2)==1 && hub.snapshot(2)->id.value()==2 && hub.snapshot(2)->identity->nickname()=="User", "Reconnect failed to own new lifecycle");
    hub.receive(2,parse("HSTA 200 Goodbye"));
    require(hub.must_close(2) && hub.snapshot(2)->phase==users::Phase::closing &&
            hub.snapshot(2)->forced_cause==users::CloseCause::peer_status && hub.session_model(2).was_ready(),"Peer fatal lost history");
    hub.disconnect(2);
    hub.connect(3,"127.0.0.1",users::Transport::adcs);
    hub.close(3,adc::CloseReason::login_timeout);
    require(hub.must_close(3) && hub.output(3).empty() && !hub.session_model(3).was_ready(),"Unready TLS queued protocol output");
    hub.disconnect(3,false,users::CloseCause::handshake_timeout);
}
void premature_and_overflow() {
    adc::LoginHub hub;
    hub.connect(1,"127.0.0.1");const auto sid=identify(hub,1);hub.receive(1,info(sid));
    hub.receive(1,parse("BMSG "+sid+" Early"));
    require(hub.snapshot(1)->phase==users::Phase::closing && !hub.session_model(1).was_ready() && drain(hub,1)==0,
            "Cancelled welcome fabricated readiness");hub.disconnect(1);
    adc::LoginHub limited({2,128,256});limited.connect(1,"127.0.0.1");const auto small=identify(limited,1);
    auto oversized=info(small);oversized.parameters.push_back("DE"+std::string(120,'x'));
    limited.receive(1,oversized);
    require(limited.must_close(1) && limited.snapshot(1)->forced_cause==users::CloseCause::output_limit &&
            !limited.snapshot(1)->identity && !limited.session_model(1).was_ready(),"Failed welcome published model identity");
    limited.disconnect(1);require(limited.queued_bytes()==0 && limited.users()==0,"Overflow cleanup leak");
}
}
int main() {
    try { adapter();premature_and_overflow();std::cout<<"PASS: ADC/ADCS session model composition, welcome, close, stale IDs and snapshots\n"; }
    catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
