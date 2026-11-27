#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/adc/serializer.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace dc24h::adc;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
Message parse(std::string_view text) {
    auto result = parse_line(text);
    require(std::holds_alternative<Message>(result), "Invalid fixture");
    return std::get<Message>(std::move(result));
}
std::vector<std::string> drain(LoginHub& hub, LoginHub::Id id, bool bytewise = false) {
    std::vector<std::string> lines;
    std::string current;
    while (!hub.output(id).empty() && !hub.must_close(id)) {
        const auto view = hub.output(id);
        const auto size = bytewise ? 1U : view.size();
        current.append(view.substr(0, size));
        hub.consume(id, size);
        if (current.back() == '\n') { lines.push_back(std::move(current)); current.clear(); }
    }
    return lines;
}
std::string handshake(LoginHub& hub, LoginHub::Id id) {
    hub.connect(id, "127.0.0.1");
    hub.receive(id, parse("HSUP ADBASE ADTIGR"));
    const auto reply = hub.output(id);
    require(reply == "ISUP ADBASE ADTIGR\n", "SUP output");
    hub.consume(id, reply.size()-1);
    require(hub.state(id) == ConnectionState::protocol, "Partial SUP advanced state");
    hub.consume(id, 1);
    const std::string sid_line(hub.output(id));
    require(sid_line.starts_with("ISID "), "Missing managed SID");
    hub.consume(id, sid_line.size()-1);
    require(hub.state(id) == ConnectionState::protocol, "Partial SID advanced state");
    hub.consume(id, 1);
    require(hub.state(id) == ConnectionState::identify, "Missing IDENTIFY");
    return sid_line.substr(5,4);
}
Message identity(std::string_view sid, unsigned seed, std::string_view nickname) {
    TigerDigest pid{};
    for (std::size_t i=0; i<pid.size(); ++i) { pid[i] = static_cast<std::uint8_t>(i+seed); }
    return parse("BINF " + std::string(sid) + " NI" + std::string(nickname) + " ID" +
        tiger_base32(*tiger192(pid)) + " PD" + tiger_base32(pid) + " I40.0.0.0 DEHello\\sworld XXprivate");
}
void serialization() {
    for (const auto* line : {"HSUP ADBASE ADTIGR", "IINF CT32 NIHub", "BMSG ABC2 Cześć\\s😀\\n\\\\",
             "DMSG ABC2 XYZ7 hello PMABC2", "FMSG ABC2 +TCP4-NAT0 hello", "CGET file /a 0 -1", "HZ99 future"}) {
        auto message = parse(line);
        auto result = serialize(message);
        require(std::holds_alternative<std::string>(result) && std::get<std::string>(result) == std::string(line)+'\n', "Round trip/escaping");
    }
    auto message = parse("IMSG ok");
    message.parameters[0] = std::string(max_frame_bytes-5, 'x');
    require(std::get<std::string>(serialize(message)).size() == max_frame_bytes+1, "Exact output bound");
    message.parameters[0] += 'x';
    require(std::get<SerializeError>(serialize(message)) == SerializeError::frame_too_long, "Oversize output accepted");
    for (const auto& text : {std::string("\xff"), std::string("a\0b",3), std::string("e\xcc\x81"), std::string(1,'\r'), std::string{}}) {
        message.parameters[0] = text;
        require(std::holds_alternative<SerializeError>(serialize(message)), "Invalid outbound text accepted");
    }
    message = parse("HSUP ADBASE"); message.source = "ABC2";
    require(std::holds_alternative<SerializeError>(serialize(message)), "Extraneous header accepted");
    message = parse("HSUP ADBASE"); message.kind = Command::msg;
    require(std::holds_alternative<SerializeError>(serialize(message)), "Mismatched kind accepted");
}
void guests() {
    LoginHub hub;
    const auto one = handshake(hub,1), two = handshake(hub,2);
    require(one != two, "SID collision");
    hub.receive(1, identity(one,1,"Żółw"));
    require(hub.state(1) == ConnectionState::normal && !hub.ready(1), "Admission/readiness ordering");
    // Both admissions before either welcome completes: no missing user lists.
    hub.receive(2, identity(two,2,"Second"));
    auto first = drain(hub,1,true), second = drain(hub,2,true);
    require(first.size()==4 && second.size()==4 && hub.ready(1) && hub.ready(2), "Concurrent welcome/join");
    require(first[1].starts_with("BINF "+one) && first[2].starts_with("IMSG Welcome") && first[3].starts_with("BINF "+two), "First list ordering");
    require(second[1].starts_with("BINF "+one) && second[2].starts_with("BINF "+two), "Own INF is not last");
    for (const auto& list : {first,second}) {
        for (const auto& line : list) { require(line.find(" PD")==std::string::npos && line.find("XX")==std::string::npos, "Private field leaked"); }
    }
    require(first[1].find("I4127.0.0.1") != std::string::npos, "Unnormalized peer address");
    hub.receive(1,parse("BINF "+one+" DE NIrenamed XX"));
    first=drain(hub,1); second=drain(hub,2);
    require(first==second && first.size()==1 && first[0].find(" DE ")!=std::string::npos && first[0].find("XX")==std::string::npos &&
        first[0].find(" ID")==std::string::npos, "Incremental update cancellation/privacy");
    hub.receive(1,parse("HMSG no-dispatch"));
    require(drain(hub,1).front().starts_with("ISTA 125 "), "Unimplemented dispatch claimed");
    require(hub.output(2).empty(), "Unsupported hub command was forwarded");
    hub.disconnect(1);
    require(drain(hub,2)==std::vector<std::string>{"IQUI "+one+'\n'}, "Presence cleanup");
    hub.disconnect(2); hub.disconnect(2);
    require(hub.users()==0 && hub.queued_bytes()==0, "Registry/queue leak");
}
void rejection() {
    LoginHub hub;
    const auto one=handshake(hub,1);
    hub.receive(1,identity(one,1,"Taken")); drain(hub,1);
    auto duplicate=[&](std::uint64_t id, unsigned seed, const char* nick, std::string_view status) {
        const auto sid=handshake(hub,id);
        hub.receive(id,identity(sid,seed,nick));
        require(hub.closing(id), "Duplicate accepted");
        const auto frames=drain(hub,id);
        require(frames.size()==2 && frames[0].starts_with("ISTA "+std::string(status)) && frames[1].starts_with("IQUI "+sid+" "), "Wrong duplicate status");
        require(hub.must_close(id) && !hub.ready(id), "Rejected login became ready");
        hub.disconnect(id);
        require(hub.users()==1 && hub.output(1).empty(), "Rejected guest announced/removed another owner");
    };
    duplicate(2,2,"Taken","222"); duplicate(3,1,"Other","224");
    const auto sid=handshake(hub,4);
    auto bad=identity(sid,4,"Bad");
    for (auto& token : bad.parameters) { if (token.starts_with("PD")) { token="PD"+std::string(39,'A'); } }
    hub.receive(4,bad);
    require(drain(hub,4)[0].starts_with("ISTA 227"), "Bad Tiger proof accepted"); hub.disconnect(4);
    hub.receive(1,parse("BINF "+one+" NIChanged")); drain(hub,1);
    const auto other=handshake(hub,5); hub.receive(5,identity(other,5,"Taken")); drain(hub,5); drain(hub,1);
    hub.receive(5,parse("BMSG "+one+" spoof"));
    require(hub.closing(5), "Spoofed source accepted"); hub.disconnect(5); drain(hub,1);
    // Fatal inbound status clears ClientSession; presence still retains its SID.
    hub.receive(1,parse("HSTA 200 Goodbye")); require(hub.must_close(1), "Fatal status ignored"); hub.disconnect(1);
    require(hub.users()==0 && hub.queued_bytes()==0, "Failure cleanup leak");
    hub.connect(6,"127.0.0.1"); hub.receive(6,parse("HSUP ADBASE"));
    require(drain(hub,6)[0].starts_with("ISTA 247"), "Missing hash accepted"); hub.disconnect(6);
    hub.connect(7,"127.0.0.1"); hub.receive(7,parse("HSUP ADBASE ADTIGR"));
    const auto pinned=hub.output(7); const auto* pointer=pinned.data();
    hub.receive(7,parse("BINF AAAB NIearly"));
    require(hub.closing(7) && hub.output(7).data()==pointer && hub.output(7)==pinned, "Pipeline bypass/TLS retry storage mutation");
    drain(hub,7); require(!hub.ready(7), "Pipeline became ready"); hub.disconnect(7);
}
void output_limits() {
    LoginHub hub({8,128,256});
    auto sid=handshake(hub,1); hub.receive(1,identity(sid,1,"Limit"));
    require(hub.must_close(1) && hub.queued_bytes()<=128, "Per-client queue bound"); hub.disconnect(1);
    require(hub.queued_bytes()==0 && hub.users()==0, "Overflow cleanup");
    LoginHub total({8,256,256});
    const auto one=handshake(total,1), two=handshake(total,2);
    total.receive(1,identity(one,1,"First"));
    total.receive(2,identity(two,2,"Second"));
    require(total.queued_bytes()<=256 && (total.must_close(1)||total.must_close(2)), "Aggregate queue bound");
    total.disconnect(1); total.disconnect(2); require(total.queued_bytes()==0, "Aggregate accounting leak");
}
}
int main() {
    try { serialization(); guests(); rejection(); output_limits(); std::cout << "PASS: bounded serializer, ordered guest login, registry, updates, rejection and cleanup\n"; }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
