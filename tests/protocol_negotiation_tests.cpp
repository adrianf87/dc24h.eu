#include "dc24h/adc/connection_routing.hpp"
#include "dc24h/adc/ctm_routing.hpp"
#include "dc24h/adc/rcm_routing.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/status.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long fail_after = -1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_after == 0) { throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* memory = std::malloc(size == 0 ? 1 : size)) { return memory; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* memory) noexcept { std::free(memory); }
[[gnu::noinline]] void operator delete[](void* memory) noexcept { std::free(memory); }
[[gnu::noinline]] void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
[[gnu::noinline]] void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using namespace dc24h;
using namespace dc24h::adc;
void require(bool value, const char* why) { if (!value) { throw std::runtime_error(why); } }
Message parse(std::string_view line) { return std::get<Message>(parse_line(line)); }
users::SessionId id(std::uint64_t n) { return *users::SessionId::from_value(n); }
users::Profile profile(std::vector<std::string> features = {"TCP4"}, std::string v4 = "127.0.0.1", std::string v6 = "::1") {
    users::ProfileData data; data.features = std::move(features); data.ipv4 = std::move(v4); data.ipv6 = std::move(v6);
    return *users::Profile::create(std::move(data));
}
void add(users::SessionManager& sessions, unsigned n, std::string sid, users::Profile p = profile()) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n), users::Transport::adc, {})), "Add failed");
    require(!sessions.start_negotiation(id(n), {}), "Start failed");
    require(!sessions.identify(id(n), *users::User::create(*users::Identity::create(sid, "CID"+std::to_string(n), "Nick"+std::to_string(n)), std::move(p)), {}), "Identify failed");
    require(!sessions.mark_ready(id(n), {}), "Ready failed");
}
void update(users::SessionManager& s, unsigned n, users::Profile p) {
    require(!s.update_user(id(n), *users::User::create(*s.find(id(n))->identity(), std::move(p)), {}), "Update failed");
}
void catalog_and_selection() {
    using namespace transfers;
    for (const auto kind : {ProtocolKind::adc_1_0, ProtocolKind::adcs_1_0}) {
        const auto name = *protocol_name(kind);
        auto protocol = *Protocol::create(name);
        require(protocol.kind() == kind && protocol.name() == name, "Known protocol changed");
        require(protocol.encryption() == (kind == ProtocolKind::adc_1_0 ? Encryption::plaintext : Encryption::tls_required), "TLS requirement changed");
    }
    for (const auto name : {"adc/1.0", "ADCS/1.00", "ADCS/0.10", "ADC0/0.10", "ADC/1.0suffix", "Unknown protocol", "Żółw"}) {
        const auto protocol = *Protocol::create(name);
        require(protocol.name() == name && protocol.kind() == ProtocolKind::extension && protocol.encryption() == Encryption::peer_defined, "Extension inferred plaintext/TLS or rewritten");
    }
    std::string text(32, 'P'); auto owned = *Protocol::create(text); text[0] = 'Q';
    require(owned.name()[0] == 'P', "Protocol borrowed storage");
    for (const auto& bad : {std::string{}, std::string(33,'P'), std::string("bad\n"), std::string("bad\t"), std::string("e\xcc\x81"), std::string("a\0b",3), std::string("\xff")}) {
        require(!Protocol::create(bad), "Invalid protocol accepted");
    }
    struct Example { Protocols a, b; ProtocolPreference preference; ProtocolKind expected; };
    const Example examples[]{
        {{true,true},{true,true},ProtocolPreference::prefer_adc,ProtocolKind::adc_1_0},
        {{true,true},{true,true},ProtocolPreference::prefer_adcs,ProtocolKind::adcs_1_0},
        {{true,true},{true,true},ProtocolPreference::require_adcs,ProtocolKind::adcs_1_0},
        {{true,true},{true,false},ProtocolPreference::prefer_adcs,ProtocolKind::adc_1_0},
        {{true,false},{true,true},ProtocolPreference::prefer_adcs,ProtocolKind::adc_1_0},
        {{false,true},{true,true},ProtocolPreference::prefer_adc,ProtocolKind::adcs_1_0},
    };
    auto modern = profile({"ADCS", "TCP4"}); auto legacy = profile({"ADC0", "UDP4"}); auto lookalike = profile({"ADCT", "TLS1"});
    fail_after = 0;
    const auto modern_flags = advertised_protocols(modern), legacy_flags = advertised_protocols(legacy), unrelated_flags = advertised_protocols(lookalike);
    for (const auto& example : examples) {
        const auto actual = negotiate_protocol(example.a, example.b, example.preference);
        require(std::get<ProtocolKind>(actual) == example.expected, "Protocol selection differs");
    }
    const auto required = negotiate_protocol({true,true},{true,false},ProtocolPreference::require_adcs);
    const auto no_common = negotiate_protocol({true,false},{false,true},ProtocolPreference::prefer_adcs);
    const auto invalid = negotiate_protocol({true,true},{true,true},static_cast<ProtocolPreference>(99));
    fail_after = -1;
    require(modern_flags == Protocols{true,true} && legacy_flags == Protocols{true,false} && unrelated_flags == Protocols{true,false}, "SU projection inferred modern TLS");
    require(std::get<NegotiationError>(required) == NegotiationError::encryption_unavailable &&
        std::get<NegotiationError>(no_common) == NegotiationError::no_common_protocol &&
        std::get<NegotiationError>(invalid) == NegotiationError::invalid_preference, "Failed selection downgraded TLS");
    require(!protocol_name(ProtocolKind::extension) && !protocol_name(static_cast<ProtocolKind>(99)) &&
        encryption(static_cast<ProtocolKind>(99)) == Encryption::peer_defined, "Unknown kind became a supported protocol");
}
void explicit_routing() {
    using namespace transfers;
    users::SessionManager sessions(8); add(sessions,1,"AAAA"); add(sessions,2,"BBBB",profile({})); add(sessions,3,"CCCC");
    Core core(sessions); ConnectionRouter router(core);
    for (bool reverse : {false,true}) for (bool echo : {false,true}) for (const auto name : {"ADC/1.0", "ADCS/1.0", "future/1.0", "adcs/1.0"}) {
        update(sessions,1,profile(reverse ? std::vector<std::string>{"ADCS"} : std::vector<std::string>{"TCP4","ADCS"}));
        update(sessions,2,profile(reverse ? std::vector<std::string>{"TCP4","ADCS"} : std::vector<std::string>{"ADCS"}));
        const auto wire = std::string(echo?"E":"D")+(reverse?"RCM":"CTM")+" AAAA BBBB "+name+" "+(reverse?"":"001515 ")+"Żółw\\sProtocolPrivate XXopaque";
        auto parsed = parse(wire);
        if (reverse) { require(std::get<RcmRequest>(validate_rcm(parsed)).protocol_kind() == classify_protocol(name), "RCM classifier differs"); }
        else { require(std::get<CtmRequest>(validate_ctm(parsed)).protocol_kind() == classify_protocol(name), "CTM classifier differs"); }
        auto prepared = router.prepare(id(1),parsed); auto route = std::get<RoutedConnection>(std::move(prepared));
        parsed.parameters[0] = "mutated";
        require(route.plan().request().protocol_value().kind() == classify_protocol(name) && route.frame()->protocol_kind() == classify_protocol(name) &&
            route.frame()->encryption() == encryption(classify_protocol(name)), "Core/wire protocol mismatch");
        // Public claims are advisory to forwarding. Neither a hub transport nor
        // a changed advertisement can silently select a different protocol.
        update(sessions,1,profile(reverse ? std::vector<std::string>{} : std::vector<std::string>{"TCP4"}));
        update(sessions,2,profile(reverse ? std::vector<std::string>{"TCP4"} : std::vector<std::string>{}));
        unsigned calls = 0;
        const auto expected_wire = wire+'\n';
        const ConnectionRouter::Sink sink = [&](auto recipient,const auto& frame) {
            require(frame->wire() == expected_wire && (recipient == id(2) || (echo && recipient == id(1))), "Negotiation changed protocol/token/audience"); ++calls; return true;
        };
        fail_after = 0; const auto report = router.dispatch(route,sink); fail_after = -1;
        require(std::get<ConnectionDispatchReport>(report).queued == (echo?2U:1U) && calls == (echo?2U:1U), "Negotiation dispatch allocated or leaked");
    }
}
void failure_tokens() {
    for (const auto code : {"141", "142", "241", "242"}) {
        const auto line = std::string("DSTA AAAA BBBB ")+code+" Peer\\snegotiation\\sfailed PRADCS/1.0 TOŻółw\\sProtocolPrivate\\\\x XXdrop";
        const auto message = parse(line);
        require(std::holds_alternative<StatusView>(validate_status(message)), "Opaque connection failure token refused");
        const auto safe = public_status(message);
        require(std::get<std::string>(serialize(safe)) == line.substr(0,line.find(" XXdrop"))+'\n', "Peer failure changed protocol/token or exposed unknown fields");
        for (bool token : {false,true}) {
            auto invalid = message; invalid.parameters[token?3:2] = std::string(token?"TO":"PR")+std::string(token?129U:33U,'P');
            require(std::get<StatusError>(validate_status(invalid)) == StatusError::invalid_field, "Oversized negotiation metadata accepted");
        }
        auto duplicate = message; duplicate.parameters.emplace_back("TOanother");
        require(std::get<StatusError>(validate_status(duplicate)) == StatusError::duplicate_field, "Ambiguous failure token accepted");
    }
    require(std::holds_alternative<StatusError>(validate_status(parse("DSTA AAAA BBBB 141 Bad PRADC\\s1.0 TOtoken"))), "PR wire grammar weakened");
    require(std::holds_alternative<StatusError>(validate_status(parse("DSTA AAAA BBBB 141 Bad PRADC/1.0 TObad\\ntoken"))), "Control in failure token accepted");
    require(std::holds_alternative<StatusError>(validate_status(parse("DSTA AAAA BBBB 100 Generic TOwith\\sspace"))), "Unrelated status token behavior changed");
}
}
int main() {
    try { catalog_and_selection(); explicit_routing(); failure_tokens(); std::cout << "PASS: protocol negotiation, TLS intent, exact routing and peer failure tokens\n"; }
    catch (const std::exception& error) { fail_after=-1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
