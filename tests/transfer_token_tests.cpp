#include "dc24h/transfers/token.hpp"
#include "dc24h/adc/connection_routing.hpp"
#include "dc24h/adc/ctm.hpp"
#include "dc24h/adc/rcm.hpp"
#include "dc24h/adc/status.hpp"
#include "dc24h/adc/serializer.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long fail_after = -1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_after == 0) { throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* p = std::malloc(size ? size : 1)) { return p; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace dc24h;
using namespace dc24h::transfers;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(unsigned n) { return *users::SessionId::from_value(n); }
adc::Message parse(std::string_view value) { return std::get<adc::Message>(adc::parse_line(value)); }
std::vector<std::string> valid_tokens() {
    return {"A", " ", "  keep case  ", "TransferPrivate Żółw \\😀", "TOpositional", "PDopaque", "00001", "Case", "case",
        std::string(128, 'T'), std::string(128, ' '), std::string(128, '\\'), std::string(124, 'T')+"😀"};
}
std::vector<std::string> invalid_tokens() {
    return {"", std::string(129, 'T'), std::string(125, 'T')+"😀", "a\nb", "a\rb", "a\tb", std::string("a\0b",3),
        "\x7f", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x80\x8b", "\xe2\x80\xa8", "e\xcc\x81"};
}
void token_values() {
    for (auto value : valid_tokens()) {
        require(!validate_token(value), "Valid opaque token refused");
        const auto original=value;
        auto owned=std::get<Token>(Token::create(value));
        value.assign("mutated");
        require(owned.value()==original, "Token borrowed mutable storage");
        auto copy=owned; auto moved=std::move(owned);
        require(copy==moved && copy.value()==original, "Token copy/move changed bytes");
    }
    require(std::get<Token>(Token::create("Case"))!=std::get<Token>(Token::create("case")), "Token case folded");
    for (const auto& value : invalid_tokens()) {
        const auto expected=value.empty()?TokenError::empty:value.size()>128?TokenError::too_long:TokenError::invalid_text;
        require(validate_token(value)==expected && std::get<TokenError>(Token::create(value))==expected, "Typed validation differs");
        RequestData input; input.port=1515; input.token=value;
        require(std::get<RequestError>(Request::create(input))==RequestError::invalid_token, "Request bypassed token policy");
    }
    fail_after=0;
    require(validate_token("")==TokenError::empty && std::get<TokenError>(Token::create(""))==TokenError::empty, "Empty token allocated");
    fail_after=-1;
    std::string oversized(1024*1024, 'T');
    fail_after=0; const auto result=Token::create(oversized); fail_after=-1;
    require(std::get<TokenError>(result)==TokenError::too_long, "Oversized token copied before bounds");
    const std::string maximum(128, 'T');
    bool failed=false;
    try { fail_after=0; (void)Token::create(maximum); } catch(const std::bad_alloc&) { failed=true; }
    fail_after=-1;
    require(failed && maximum==std::string(128,'T'), "Owning allocation failure swallowed or mutated input");
}
void codec_parity() {
    for (const auto& token : valid_tokens()) for (bool reverse : {false,true}) for (bool echo : {false,true}) {
        auto message=parse(std::string(echo?"E":"D")+(reverse?"RCM":"CTM")+" AAAA BBBB ADC/1.0 "+(reverse?"":"01515 ")+"placeholder TOextension");
        message.parameters[reverse?1U:2U]=token;
        const auto wire=std::get<std::string>(adc::serialize(message));
        if (reverse) {
            const auto native=std::get<adc::RcmRequest>(adc::validate_rcm(message));
            const auto decoded=std::get<adc::RcmRequest>(adc::parse_rcm(std::string_view(wire).substr(0,wire.size()-1)));
            require(native.token()==token && decoded.token()==token, "RCM changed positional token");
        } else {
            const auto native=std::get<adc::CtmRequest>(adc::validate_ctm(message));
            const auto decoded=std::get<adc::CtmRequest>(adc::parse_ctm(std::string_view(wire).substr(0,wire.size()-1)));
            require(native.token()==token && decoded.token()==token, "CTM changed positional token");
        }
        for (const auto code : {"141","142","241","242"}) {
            auto status=parse(std::string(echo?"ESTA":"DSTA")+" BBBB AAAA "+code+" Failed PRADC/1.0 TOplaceholder XXprivate");
            status.parameters[3]="TO"+token;
            require(std::holds_alternative<adc::StatusView>(adc::validate_status(status)), "Peer failure token refused");
            const auto safe=adc::public_status(status);
            require(safe.parameters.size()==4 && safe.parameters[3]=="TO"+token, "Peer failure token rewritten");
            auto duplicate=status; duplicate.parameters.emplace_back("TOanother");
            require(std::get<adc::StatusError>(adc::validate_status(duplicate))==adc::StatusError::duplicate_field, "Duplicate status TO accepted");
        }
    }
    for (const auto& token : invalid_tokens()) {
        auto ctm=parse("DCTM AAAA BBBB ADC/1.0 1515 valid"); ctm.parameters[2]=token;
        auto rcm=parse("DRCM AAAA BBBB ADC/1.0 valid"); rcm.parameters[1]=token;
        auto sta=parse("DSTA BBBB AAAA 142 Failed TOvalid"); sta.parameters[2]="TO"+token;
        require(std::holds_alternative<adc::CtmError>(adc::validate_ctm(ctm)) &&
            std::holds_alternative<adc::RcmError>(adc::validate_rcm(rcm)) &&
            std::holds_alternative<adc::StatusError>(adc::validate_status(sta)), "Codec bypassed shared token policy");
    }
}
void add(users::SessionManager& sessions, unsigned n) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n), users::Transport::adc, {})), "Add failed");
    require(!sessions.start_negotiation(id(n), {}), "Negotiate failed");
    users::ProfileData data; data.ipv4="127.0.0.1"; if(n==1) {data.features={"TCP4"};}
    require(!sessions.identify(id(n), *users::User::create(*users::Identity::create(n==1?"AAAA":"BBBB", "CID"+std::to_string(n), "User"+std::to_string(n)), *users::Profile::create(data)), {}), "Identify failed");
    require(!sessions.mark_ready(id(n), {}), "Ready failed");
}
void immutable_forwarding_and_faults() {
    users::SessionManager sessions(4); add(sessions,1); add(sessions,2);
    Core core(sessions); adc::ConnectionRouter router(core);
    for(const auto& token : valid_tokens()) for(bool reverse : {false,true}) for(bool echo : {false,true}) {
        auto message=parse(std::string(echo?"E":"D")+(reverse?"RCM BBBB AAAA":"CTM AAAA BBBB")+" future/1.0 "+(reverse?"":"01515 ")+"placeholder TOextension");
        message.parameters[reverse?1U:2U]=token;
        const auto encoded=std::get<std::string>(adc::serialize(message));
        auto route=std::get<adc::RoutedConnection>(router.prepare(id(reverse?2U:1U),message));
        message.parameters[reverse?1U:2U]="changed";
        auto copy=route;
        unsigned calls=0;
        const adc::ConnectionRouter::Sink sink=[&](auto who,const auto& frame) {
            require(who==id(reverse?1U:2U)||(echo&&who==id(reverse?2U:1U)), "Token selected a bystander");
            require(frame->wire()==encoded && frame->token()==token && route.plan().request().token_value().value()==token, "Retained token changed");
            ++calls; return true;
        };
        fail_after=0; const auto report=router.dispatch(route,sink); fail_after=-1;
        require(std::get<adc::ConnectionDispatchReport>(report).queued==(echo?2U:1U), "Token forwarding failed");
        require(std::get<adc::ConnectionRoutingError>(router.dispatch(copy,sink))==adc::ConnectionRoutingError::already_dispatched, "Copied attempt replayed");
        // Reusing the same opaque bytes in a new independent request is legal.
        message.parameters[reverse?1U:2U]=token;
        const auto fresh=std::get<adc::RoutedConnection>(router.prepare(id(reverse?2U:1U),message));
        require(router.cancel(fresh) && calls==(echo?2U:1U), "Token became a global uniqueness key");
    }
    auto message=parse("ECTM AAAA BBBB ADC/1.0 1515 "+std::string(128,'T'));
    bool succeeded=false;
    unsigned failures=0;
    for(long budget=0;budget<100 && !succeeded;++budget) {
        try {
            fail_after=budget; auto result=router.prepare(id(1),message); fail_after=-1;
            const auto route=std::get<adc::RoutedConnection>(std::move(result));
            require(route.frame()->token()==std::string(128,'T') && router.cancel(route), "Fault changed token or attempt"); succeeded=true;
        } catch(const std::bad_alloc&) { fail_after=-1; ++failures; }
        require(core.capabilities(id(1)).has_value() && core.capabilities(id(2)).has_value(), "Fault mutated sessions");
    }
    require(succeeded && failures>0, "Preparation fault coverage missing");
}
}
int main() {
    try { token_values(); codec_parity(); immutable_forwarding_and_faults(); std::cout << "Transfer token checks passed\n"; }
    catch(const std::exception& error) { fail_after=-1; std::cerr << error.what() << '\n'; return 1; }
}
