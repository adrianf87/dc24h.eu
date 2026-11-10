#include "dc24h/adc/status.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/serializer.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace dc24h::adc;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
Message parse(std::string_view body) {
    auto parsed=parse_line(body); require(std::holds_alternative<Message>(parsed), "Fixture syntax");
    return std::get<Message>(std::move(parsed));
}
std::vector<std::string> drain(LoginHub& hub, LoginHub::Id id, bool bytewise=false) {
    std::vector<std::string> lines;
    std::string line;
    while (!hub.output(id).empty() && !hub.must_close(id)) {
        const auto bytes=hub.output(id);
        const auto size=bytewise ? 1U : bytes.size();
        line.append(bytes.substr(0,size)); hub.consume(id,size);
        if (line.back()=='\n') { lines.push_back(std::move(line)); line.clear(); }
    }
    return lines;
}
std::string identify(LoginHub& hub, LoginHub::Id id) {
    hub.connect(id,"127.0.0.1"); hub.receive(id,parse("HSUP ADBASE ADTIGR"));
    auto lines=drain(hub,id,true); require(lines.size()==2, "Handshake replies");
    return lines.back().substr(5,4);
}
std::string login(LoginHub& hub, LoginHub::Id id, unsigned seed, const char* features="BASE") {
    const auto sid=identify(hub,id);
    TigerDigest pid{}; pid[0]=static_cast<std::uint8_t>(seed);
    hub.receive(id,parse("BINF "+sid+" NIUser"+std::to_string(seed)+" ID"+tiger_base32(*tiger192(pid))+
        " PD"+tiger_base32(pid)+" SU"+features));
    drain(hub,id); require(hub.ready(id), "Guest not ready"); return sid;
}
void codes_and_fields() {
    for (unsigned i=0; i<1000; ++i) {
        const auto text=std::to_string(1000+i).substr(1);
        const auto code=StatusCode::parse(text);
        const bool valid=i==0 || (i>=100 && i<300);
        require(code.has_value()==valid, "Status severity/success domain");
        if (code) { require(code->str()==text && code->error()==i%100, "Status round trip"); }
    }
    for (const auto* text : {"", "00", "0000", "2A0", "+20", "200\n"}) { require(!StatusCode::parse(text), "Invalid code accepted"); }
    for (const auto* text : {"HSTA 000 OK FCHSUP", "ISTA 199 Unknown", "DSTA AAAB AAAC 242 Failed TOtoken PRADC/1.0",
                            "ISTA 243 Bad FMNI FBID", "ISTA 246 Bad I4127.0.0.1 I6::1", "ISTA 232 Wait TL9223372036854775807"}) {
        require(std::holds_alternative<StatusView>(validate_status(parse(text))), "Valid STA rejected");
    }
    for (const auto* text : {"HSTA 001 Bad", "HSTA 999 Bad", "HSTA 200 Bad FChsup", "HSTA 200 Bad FC",
                            "HSTA 200 Bad FCHMSG FCHSUP", "HSTA 200 Bad TL-1", "HSTA 200 Bad TL9223372036854775808",
                            "HSTA 200 Bad FMNIXX", "HSTA 200 Bad I4127.0.0.1:80", "HSTA 200 Bad I6garbage",
                            "HSTA 200 Bad PRADC\\s1.0"}) {
        require(std::holds_alternative<StatusError>(validate_status(parse(text))), "Invalid STA accepted");
    }
    auto status=make_status(*StatusCode::parse("125"),"Nie ma 😀\nBack\\slash", "BMSG");
    const auto wire=std::get<std::string>(serialize(status));
    require(wire.find("Nie\\sma\\s😀\\nBack\\\\slash")!=std::string::npos, "Description escaping");
    status.parameters[1]=std::string(max_status_text_bytes,'x');
    require(std::holds_alternative<StatusView>(validate_status(status)), "Exact description bound");
    status.parameters[1]+='x'; require(std::holds_alternative<StatusError>(validate_status(status)), "Description bound bypass");
    status=parse("DSTA AAAB AAAC 199 Future XXprivate PDprivate FCDMSG");
    const auto safe=public_status(status);
    require(safe.parameters==std::vector<std::string>{"199","Future","FCDMSG"}, "Unknown/private status fields leaked");
    require(status.parameters.size()==5, "Sanitizer changed caller input");
}
void quit_fields() {
    const auto sid=*Sid::parse("AAAB");
    auto quit=make_quit(sid, {*Sid::parse("AAAC"),-1,"Reason 😀", "adcs://hub.invalid:1512/",true});
    const auto view=std::get<QuitView>(validate_quit(quit));
    require(view.sid==sid && view.initiator==Sid::parse("AAAC") && view.reconnect_after==-1 && view.unwanted &&
            view.message=="Reason 😀" && view.redirect=="adcs://hub.invalid:1512/", "QUIT flags lost");
    require(std::holds_alternative<QuitView>(validate_quit(parse("IQUI AAAB TL0 MS XXfuture"))), "Optional/unknown QUI fields");
    for (const auto* body : {"IQUI AAAB TL-2", "IQUI AAAB TL9223372036854775808", "IQUI AAAB IDbad", "IQUI AAAB TL0 TL1",
                            "IQUI AAAB RD", "HQUI AAAB", "BQUI AAAC AAAB"}) {
        require(std::holds_alternative<StatusError>(validate_quit(parse(body))), "Invalid QUI accepted");
    }
    ConnectionStateMachine state;
    require(!state.observe(Direction::from_client,parse("HSUP ADBASE")) && !state.approve_protocol(), "Protocol fixture");
    require(!state.observe(Direction::to_client,parse("ISUP ADBASE")) &&
            !state.observe(Direction::to_client,parse("ISID AAAB")), "Identify fixture");
    require(state.observe(Direction::from_client,parse("HSTA 001 Bad"))==StateError::invalid_message &&
            state.state()==ConnectionState::identify, "Malformed status changed state");
    require(!state.observe(Direction::from_client,parse("HSTA 000 OK")) && state.state()==ConnectionState::identify,
            "Success bypassed identity approval");
    require(state.observe(Direction::to_client,parse("IQUI AAAB TL-2"))==StateError::invalid_message &&
            state.state()==ConnectionState::identify, "Malformed QUI disconnected");
    require(!state.observe(Direction::to_client,quit) && state.state()==ConnectionState::disconnected, "Own QUI not terminal");
}
void terminal_ordering() {
    LoginHub hub;
    const auto one=login(hub,1,1), two=login(hub,2,2); drain(hub,1);
    hub.receive(1,parse("BINF "+one+" DEfirst"));
    hub.receive(1,parse("BINF "+one+" DEcancelled"));
    const std::string front(hub.output(1)); hub.consume(1,1);
    const auto pinned=hub.output(1); const auto* pointer=pinned.data();
    hub.close(1,CloseReason::idle_timeout); hub.close(1,CloseReason::login_timeout);
    require(hub.output(1)==pinned && hub.output(1).data()==pointer && hub.closing(1) && !hub.ready(1), "Pinned front changed");
    hub.receive(1,parse("BINF "+one+" DEignored"));
    auto lines=drain(hub,1,true);
    require(lines.size()==3 && lines[0]==std::string(front).substr(1) && lines[1].starts_with("ISTA 230 ") &&
            lines[2]=="IQUI "+one+" MSIdle\\sconnection\\stimed\\sout\n", "Terminal ordering/cancel/duplicate");
    require(hub.must_close(1) && hub.output(1).empty() && hub.users()==2, "Premature identity release");
    hub.disconnect(1); hub.disconnect(1);
    const auto other=drain(hub,2);
    require(other.size()==3 && other.back()=="IQUI "+one+'\n' && hub.users()==1, "Exactly-once presence removal");
    hub.disconnect(2); require(hub.queued_bytes()==0, "Queue leak");
    const auto assigned=identify(hub,3);
    hub.receive(3,parse("HQUI "+two));
    lines=drain(hub,3,true);
    require(lines.size()==2 && lines[0].starts_with("ISTA 240 ") && lines[1].starts_with("IQUI "+assigned+" "), "Client QUI forgery");
    hub.disconnect(3);
    hub.connect(4,"127.0.0.1"); hub.close(4,CloseReason::login_timeout);
    require(drain(hub,4).size()==1, "QUI before SID assignment"); hub.disconnect(4);
    // A pinned ISID completes before terminal STA/QUI even though the module
    // has not observed its completion yet.
    hub.connect(5,"127.0.0.1"); hub.receive(5,parse("HSUP ADBASE ADTIGR"));
    hub.consume(5,hub.output(5).size());
    const std::string sid_line(hub.output(5)); hub.close(5,CloseReason::login_timeout);
    lines=drain(hub,5,true);
    require(lines.size()==3 && lines[0]==sid_line && lines.back().starts_with("IQUI "+sid_line.substr(5,4)), "Pending SID close ordering");
    hub.disconnect(5);
}
void routing_and_peer_status() {
    LoginHub hub;
    const auto one=login(hub,1,1,"BASE,TCP4"), two=login(hub,2,2), three=login(hub,3,3,"BASE,TCP4");
    drain(hub,1); drain(hub,2);
    hub.receive(1,parse("DSTA "+one+" "+two+" 299 Future FCDMSG XXprivate"));
    require(drain(hub,2)==std::vector<std::string>{"DSTA "+one+" "+two+" 299 Future FCDMSG\n"} &&
            hub.ready(1) && hub.ready(2) && hub.output(1).empty() && hub.output(3).empty(), "Fatal direct status killed hub session/leaked fields");
    hub.receive(1,parse("ESTA "+one+" "+one+" 100 Self")); require(drain(hub,1).size()==1, "Echo self duplicated");
    hub.receive(1,parse("ESTA "+one+" "+two+" 100 Echo")); require(drain(hub,1)==drain(hub,2), "Echo recipients");
    hub.receive(1,parse("FSTA "+one+" +TCP4 100 Feature"));
    require(drain(hub,1).size()==1 && drain(hub,3).size()==1 && hub.output(2).empty(), "Feature selection uses wrong capability set");
    hub.receive(1,parse("BSTA "+one+" 100 Broadcast"));
    require(drain(hub,1).size()==1 && drain(hub,2).size()==1 && drain(hub,3).size()==1, "Broadcast recipients");
    hub.receive(1,parse("DSTA "+one+" AAAA 100 Missing"));
    require(drain(hub,1).front().starts_with("ISTA 140 ") && hub.ready(1), "Missing status target disconnects");
    hub.receive(1,parse("HSTA 000 Fine")); require(hub.output(1).empty() && hub.ready(1), "Status reply loop");
    hub.receive(1,parse("HSTA 299 PrivatePeerReason")); require(hub.must_close(1) && hub.peer_fatal(1) && hub.output(1).empty(), "Peer fatal not terminal");
    hub.disconnect(1); require(drain(hub,2)==std::vector<std::string>{"IQUI "+one+'\n'} &&
                              drain(hub,3)==std::vector<std::string>{"IQUI "+one+'\n'}, "Peer description leaked into presence");
    hub.receive(2,parse("HSTA 999 Invalid")); auto lines=drain(hub,2);
    require(lines.size()==2 && lines[0].starts_with("ISTA 240 ") && lines[1].starts_with("IQUI "+two), "Invalid severity ignored");
    hub.disconnect(2); hub.disconnect(3);
    require(hub.queued_bytes()==0 && hub.users()==0, "Status cleanup leak");
}
}
int main() {
    try { codes_and_fields(); quit_fields(); terminal_ordering(); routing_and_peer_status();
        std::cout << "PASS: STA/QUI semantics, terminal ordering, routing and cleanup\n"; }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
