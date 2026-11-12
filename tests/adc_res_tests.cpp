#include "dc24h/adc/res.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/login.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
namespace { long allocation_budget = -1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (allocation_budget == 0) { throw std::bad_alloc(); }
    if (allocation_budget > 0) { --allocation_budget; }
    if (auto* value = std::malloc(size == 0 ? 1 : size)) { return value; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete(void* value, std::size_t) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value, std::size_t) noexcept { std::free(value); }


namespace {
using namespace dc24h;
using namespace dc24h::adc;
void require(bool value, const char* why) { if (!value) { throw std::runtime_error(why); } }
Message parse(std::string_view body) { return std::get<Message>(parse_line(body)); }
const std::string hash(39,'A');
const std::string head="DRES AAAB AAAC ";
ResRequest checked(std::string_view body) {
    auto result=parse_res(body); require(std::holds_alternative<ResRequest>(result),"RES unexpectedly refused");
    return std::get<ResRequest>(std::move(result));
}
void error(std::string_view body, ResError expected) {
    if (!body.empty() && body.back() == ' ') { body.remove_suffix(1); }
    const auto result=parse_res(body);
    if (!std::get_if<ResError>(&result) || std::get<ResError>(result)!=expected) {
        throw std::runtime_error("Unexpected semantic RES error: " + std::string(body));
    }
}
void fields_and_ownership() {
    const auto body=head+"XX XXone XXtwo TOŻółw\\stoken FN/share/Żółw\\s😀.txt SI00042 SL-0 TR"+hash;
    auto request=checked(body);
    require(request.item().name=="/share/Żółw 😀.txt" && request.item().bytes==42 &&
        request.item().type==search::ItemType::file && request.item().tth==TigerDigest{} &&
        request.slots()==0 && request.token()=="Żółw token","Typed result fields changed");
    require(std::get<std::string>(serialize(request.message()))==body+"\n","Original fields/order/numeric spelling lost");
    require(std::holds_alternative<search::Item>(search::Item::create(request.item())),"Result descriptor cannot project to Item");
    auto no_token=checked(head+"FNrelative.txt SI0 TR"+hash);
    require(!no_token.token() && !no_token.slots(),"Absent fields were fabricated");
    for (const auto name:{"/share/", "share/", "/share/sub/"}) {
        auto directory=checked(head+"FN"+name+" SI9223372036854775807 SL1 TOa\\\\b");
        require(directory.item().type==search::ItemType::directory && !directory.item().tth &&
            directory.item().bytes==INT64_MAX && directory.token()=="a\\b","Directory result changed");
    }
    auto input=parse(body); auto owning=std::get<ResRequest>(validate_res(input));
    input.parameters.clear(); auto copy=owning; auto moved=std::move(copy);
    require(moved.item().name==request.item().name && moved.token()==request.token() &&
        std::get<std::string>(serialize(moved.message()))==body+"\n","RES retained borrowed storage");
}
void failures_and_bounds() {
    for (const auto& fields:{std::string{},std::string("XXopaque"),std::string("FNfile SI0"),
                            std::string("FNfile TR")+hash,std::string("SI0 TR")+hash}) {
        error(head+fields,ResError::missing_field);
    }
    for (const auto name:{"", "/", ".", "..", "/a/../b", "/a/./b", "/a//b", "//a", "/a//", "a\\n"}) {
        error(head+"FN"+name+" SI0 TR"+hash,ResError::invalid_name);
    }
    for (const auto fields:{"SI", "SI-1", "SL-1", "SI+1", "SI1.0", "SI9223372036854775808"}) {
        error(head+"FNfile "+fields+" TR"+hash,ResError::invalid_integer);
    }
    for (const auto& field:{std::string("FNfile"),std::string("SI0"),std::string("SL0"),std::string("TOopaque"),"TR"+hash}) {
        error(head+"FNfile SI0 SL0 TOopaque TR"+hash+" "+field,ResError::duplicate_field);
    }
    for (const auto& value:{std::string{},std::string(38,'A'),std::string(39,'a'),std::string(38,'A')+"B"}) {
        error(head+"FNfile SI0 TR"+value,ResError::invalid_hash);
    }
    error(head+"FNdir/ SI0 TR"+hash,ResError::invalid_hash);
    error(head+"FNfile SI0 TR"+hash+" TO",ResError::invalid_token);
    error(head+"FNfile SI0 TR"+hash+" TOa\\n",ResError::invalid_token);
    (void)checked(head+"FNfile SI0 TR"+hash+" TO"+std::string(search::max_token_bytes,'x'));
    error(head+"FNfile SI0 TR"+hash+" TO"+std::string(search::max_token_bytes+1,'x'),ResError::invalid_token);
    (void)checked(head+"FN"+std::string(search::max_item_name_bytes,'x')+" SI0 TR"+hash);
    error(head+"FN"+std::string(search::max_item_name_bytes+1,'x')+" SI0 TR"+hash,ResError::invalid_name);
    auto exact=head+"FNfile SI0 TR"+hash+" XX";
    exact+=std::string(max_res_bytes-6-3-41-2,'x');
    (void)checked(exact); error(exact+"x",ResError::field_limit);
    std::string fields=head+"FNfile SI0 TR"+hash;
    for (std::size_t i=3;i<max_parameters;++i) { fields+=" XX"; }
    (void)checked(fields);
    require(std::get<ParseError>(parse_res(fields+" XX"))==ParseError::too_many_parameters,"Parameter limit bypassed");
    for (const auto& body:std::vector<std::string>{head+"FNbad\\q SI0",head+"FNbad\xff SI0",head+"FNe\xcc\x81 SI0", "DRES AAA0 AAAC FNfile SI0"}) {
        require(std::holds_alternative<ParseError>(parse_res(body)),"Syntax error lost its type");
    }
    require(std::holds_alternative<ResError>(parse_res("")),"Keepalive interpreted as result");
    for (const auto body:{"BRES AAAB FNfile SI0", "ERES AAAB AAAC FNfile SI0", "FRES AAAB +TCP4 FNfile SI0",
                         "CRES FNfile SI0", "URES AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA FNfile SI0"}) {
        error(body,ResError::invalid_type);
    }
}
void hostile_and_faults() {
    const auto body=head+"FN/share/"+std::string(128,'x')+" SI0 TR"+hash+" TOopaque XXunknown";
    for (int i=0;i<9;++i) {
        auto message=parse(body);
        switch (i) {
        case 0: message.kind=Command::sch; break;
        case 1: message.command="SCH"; break;
        case 2: message.positional_count=1; break;
        case 3: message.source.reset(); break;
        case 4: message.source="AAA0"; break;
        case 5: message.target.reset(); break;
        case 6: message.features={{true,"TCP4"}}; break;
        case 7: message.parameters.push_back("bad"); break;
        case 8: message.parameters.push_back(std::string("XXbad\0text",10)); break;
        }
        require(std::holds_alternative<ResError>(validate_res(message)),"Hostile object bypassed checks");
    }
    auto huge=parse(body); huge.parameters={std::string(1024*1024,'x')};
    allocation_budget=0; const auto bounded=validate_res(huge); allocation_budget=-1;
    require(std::get<ResError>(bounded)==ResError::field_limit,"Oversized input allocated before bounds");
    const auto message=parse(body); bool failed=false,success=false;
    for (long budget=0;budget<160 && !success;++budget) {
        allocation_budget=budget;
        try { const auto result=validate_res(message); allocation_budget=-1;
            require(std::holds_alternative<ResRequest>(result),"Fault changed result"); success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1; failed=true; }
        require(std::get<std::string>(serialize(message))==body+"\n","Failure mutated caller message");
    }
    require(failed && success,"Allocation campaign missed a path");
}
std::string drain(LoginHub& hub,std::uint64_t owner) {
    std::string result;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) { const std::string wire(hub.output(owner)); result+=wire; hub.consume(owner,wire.size()); }
    return result;
}
std::string join(LoginHub& hub,std::uint64_t owner,std::string& welcome) {
    hub.connect(owner,"127.0.0.1"); hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size()); const std::string sid(hub.output(owner).substr(5,4)); hub.consume(owner,hub.output(owner).size());
    TigerDigest pid{}; pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)));
    welcome=drain(hub,owner); require(hub.ready(owner),"Configured welcome prevented readiness"); return sid;
}

void receive_isolation() {
    LoginHub hub; std::string welcome;
    const auto source=join(hub,1,welcome), target=join(hub,2,welcome); (void)drain(hub,1);
    const auto body="DRES "+source+" "+target+" FN/share/file SI0 TR"+hash+" TOprivate";
    hub.receive(1,parse(body));
    require(drain(hub,1)=="ISTA 125 Search\\sresult\\srouting\\snot\\simplemented FCDRES\n" && hub.output(2).empty(),"Valid result implied routing");
    const auto message=parse(body);
    allocation_budget=0; hub.receive(1,message); allocation_budget=-1;
    require(hub.must_close(1) && hub.ready(2) && hub.output(2).empty(),"RES allocation failure escaped or affected another peer");
    require(hub.chat_statistics()==chat::Statistics{},"RES counted as chat");
}
}
int main() {
    try { fields_and_ownership(); failures_and_bounds(); hostile_and_faults(); receive_isolation();
        std::cout<<"PASS: typed RES, bounds, ownership and receive isolation\n"; }
    catch (const std::exception& e) { allocation_budget=-1; std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
