#include "dc24h/adc/sch.hpp"
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
SchRequest checked(std::string_view body) {
    auto result=parse_sch(body); require(std::holds_alternative<SchRequest>(result),"SCH unexpectedly refused");
    return std::get<SchRequest>(std::move(result));
}
void error(std::string_view body, SchError expected) {
    const auto result=parse_sch(body);
    require(std::get_if<SchError>(&result) && std::get<SchError>(result)==expected,"Unexpected semantic SCH error");
}
void fields_and_projection() {
    const std::string body="FSCH AAAB +TCP4-UDP4 ANUbuntu ANlinux NOgentoo EXISO EXIMG GE-0 LE9223372036854775807 EQ4096 TY01 TOopaque XX XXone XXtwo";
    auto request=checked(body); const auto& data=request.criteria();
    require(data.include==std::vector<std::string>{"Ubuntu","linux"} && data.exclude==std::vector<std::string>{"gentoo"} &&
        data.extensions==std::vector<std::string>{"ISO","IMG"} && data.min_bytes==0 && data.max_bytes==INT64_MAX &&
        data.exact_bytes==4096 && data.type==search::ItemType::file && data.token=="opaque" && !request.root(),"Typed fields changed");
    require(request.selection()==search::Selection{{"TCP4"},{"UDP4"}} &&
        std::get<std::string>(serialize(request.message()))==body+"\n","Original fields/order/numeric spelling lost");
    require(std::get<search::Query>(request.to_core_query()).data()==data,"Core textual projection differs");
    for (const auto type:{"BSCH AAAB", "DSCH AAAB AAAC", "ESCH AAAB AAAC"}) {
        auto result=checked(std::string(type)+" ANŻółw\\s😀 ANc:\\\\path NOtmp EXtar.gz TY2 TOa\\\\b");
        require(result.criteria().include==std::vector<std::string>{"Żółw 😀","c:\\path"} && result.criteria().type==search::ItemType::directory &&
            result.criteria().token=="a\\b","Decoded Unicode, escapes or type differ");
    }
    const auto unicode=checked("BSCH AAAB ANx TOŻółw\\stoken");
    require(unicode.criteria().token=="Żółw token" && std::get<search::Query>(unicode.to_core_query()).data()==unicode.criteria(),
        "Opaque ADC token was rewritten or refused");
    const auto filters=checked("BSCH AAAB EXiso GE0 TY1");
    require(std::get<search::Query>(filters.to_core_query()).data()==filters.criteria(),"Filter-only query was changed or refused");
    const auto hash=checked("BSCH AAAB TR"+std::string(39,'A')+" ANx TOhash");
    require(hash.root()==TigerDigest{} && std::get<search::Query>(hash.to_core_query()).data()==hash.criteria(),"TR was silently discarded");
    require(checked("BSCH AAAB TR"+std::string(39,'A')).root().has_value(),"Hash-only query refused");
    auto source=parse("BSCH AAAB ANOriginal TOtoken"); auto owning=std::get<SchRequest>(validate_sch(source));
    source.parameters[0]="ANChanged"; auto copy=owning; auto moved=std::move(copy);
    require(moved.criteria().include[0]=="Original" && moved.message().parameters[0]=="ANOriginal","SCH object borrowed or lost storage after copy/move");
}
void failures_and_bounds() {
    for (const auto fields:{"", " XX", " TOtoken XXopaque"}) { error(std::string("BSCH AAAB")+fields,SchError::no_criteria); }
    for (const auto fields:{"AN", "AN\\s", "ANx\\n", "NO", "NOx\\n"}) { error(std::string("BSCH AAAB ")+fields,SchError::invalid_text); }
    for (const auto fields:{"EX", "EX.iso", "EXa/b", "EXa\\\\b", "EXa\\sb"}) { error(std::string("BSCH AAAB ")+fields,SchError::invalid_extension); }
    for (const auto fields:{"GE", "LE-1", "EQ+1", "GE9223372036854775808", "GE1.0", "TY-1"}) {
        error(std::string("BSCH AAAB ANx ")+fields,SchError::invalid_integer);
    }
    for (const auto fields:{"TY0","TY3"}) { error(std::string("BSCH AAAB ANx ")+fields,SchError::invalid_file_type); }
    for (const auto fields:{"GE3 LE2","GE3 EQ2","LE2 EQ3"}) { error(std::string("BSCH AAAB ANx ")+fields,SchError::invalid_size); }
    for (const auto fields:{"TO", "TOx\\n"}) { error(std::string("BSCH AAAB ANx ")+fields,SchError::invalid_token); }
    for (const auto field:{"GE0", "LE1", "EQ1", "TY1", "TOtoken"}) {
        error("BSCH AAAB ANx "+std::string(field)+" "+field,SchError::duplicate_field);
    }
    error("BSCH AAAB TR"+std::string(39,'A')+" TR"+std::string(39,'A'),SchError::duplicate_field);
    for (const auto& value:{std::string(38,'A'),std::string(38,'A')+"B",std::string(39,'a')}) {
        error("BSCH AAAB TR"+value,SchError::invalid_hash);
    }
    (void)checked("BSCH AAAB AN"+std::string(search::max_term_bytes,'x'));
    error("BSCH AAAB AN"+std::string(search::max_term_bytes+1,'x'),SchError::invalid_text);
    (void)checked("BSCH AAAB ANx TO"+std::string(search::max_token_bytes,'t'));
    error("BSCH AAAB ANx TO"+std::string(search::max_token_bytes+1,'t'),SchError::invalid_token);
    const auto exact="BSCH AAAB ANx XX"+std::string(max_sch_bytes-5,'x');
    require(checked(exact).message().parameters.back().size()==max_sch_bytes-3,"Exact decoded payload bound refused");
    error(exact+"x",SchError::field_limit);
    std::string terms="BSCH AAAB"; for (std::size_t i=0;i<search::max_terms;++i) { terms+=" ANx"; }
    (void)checked(terms); error(terms+" NOx",SchError::field_limit);
    std::string extensions="BSCH AAAB ANx"; for (std::size_t i=0;i<search::max_extensions;++i) { extensions+=" EXiso"; }
    (void)checked(extensions); error(extensions+" EXimg",SchError::field_limit);
    std::string selectors="FSCH AAAB ";
    for (std::size_t i=0;i<search::max_selectors;++i) { selectors+="+AA"; selectors+=char('A'+i/10); selectors+=char('0'+i%10); }
    (void)checked(selectors+" ANx"); error(selectors+"+ZZZZ ANx",SchError::field_limit);
    error("FSCH AAAB +TCP4+TCP4 ANx",SchError::invalid_features);
    error("FSCH AAAB +TCP4-TCP4 ANx",SchError::invalid_features);
    std::string parameters="BSCH AAAB ANx"; for (std::size_t i=1;i<max_parameters;++i) { parameters+=" XX"; }
    (void)checked(parameters);
    require(std::get<ParseError>(parse_sch(parameters+" XX"))==ParseError::too_many_parameters,"Generic parameter cap changed");
    require(std::get<ParseError>(parse_sch(std::string(max_frame_bytes+1,'x')))==ParseError::frame_too_long,"Generic frame cap bypassed");
    for (const auto& body:std::vector<std::string>{"BSCH AAAB ANe\xcc\x81", "BSCH AAAB ANbad\\q", "BSCH AAA0 ANx", "BSCH AAAB bare", "BSCH AAAB AN"+std::string(1,char(0xff))}) {
        require(std::holds_alternative<ParseError>(parse_sch(body)),"Wire syntax failure lost its error type");
    }
}
void ncdc_file_type_filters() {
    const auto document = checked(
        "BSCH AAAB ANreport TY1 EXdoc EXdocx EXhtm EXhtml EXnfo EXodf EXodp EXods EXodt"
        " EXpdf EXppt EXpptx EXrtf EXtxt EXxls EXxlsx EXxml EXxps");
    require(document.criteria().extensions.size() == 18, "ncdc document extensions lost");
    require(std::holds_alternative<search::Query>(document.to_core_query()), "Core refused ncdc document filters");

    const auto video = checked(
        "BSCH AAAB ANclip TY1 EX3gp EXasf EXasx EXavi EXdivx EXflv EXmkv EXmov EXmp4"
        " EXmpeg EXmpg EXogm EXpxp EXqt EXrm EXrmvb EXswf EXvob EXwebm EXwmv");
    require(video.criteria().extensions.size() == 20, "ncdc video extensions lost");
    require(std::holds_alternative<search::Query>(video.to_core_query()), "Core refused ncdc video filters");
}
void hostile_objects_and_faults() {
    for (const auto type:{MessageType::hub,MessageType::info,MessageType::client,MessageType::udp}) {
        auto message=parse("BSCH AAAB ANx"); message.type=type;
        require(std::get<SchError>(validate_sch(message))==SchError::invalid_type,"Unsupported scope accepted");
    }
    for (int variant=0; variant<8; ++variant) {
        auto message=parse("BSCH AAAB ANx");
        switch (variant) {
        case 0: message.command="MSG"; break;
        case 1: message.positional_count=1; break;
        case 2: message.source.reset(); break;
        case 3: message.source="AAA0"; break;
        case 4: message.target="AAAC"; break;
        case 5: message.features={{true,"TCP4"}}; break;
        case 6: message.parameters={"bad"}; break;
        case 7: message.parameters={std::string("ANbad\0text",10)}; break;
        }
        require(std::holds_alternative<SchError>(validate_sch(message)),"Hostile Message bypassed validation");
    }
    auto huge=parse("BSCH AAAB ANx"); huge.parameters={std::string(1024*1024,'x')};
    allocation_budget=0; const auto bounded=validate_sch(huge); allocation_budget=-1;
    require(std::get<SchError>(bounded)==SchError::field_limit,"Oversized hand-built input allocated before bounds");
    const auto message=parse("FSCH AAAB +TCP4-UDP4 AN"+std::string(128,'x')+" TOtoken XXopaque");
    const auto original=std::get<std::string>(serialize(message)); bool failed=false, success=false;
    for (long budget=0;budget<150 && !success;++budget) {
        allocation_budget=budget;
        try { const auto result=validate_sch(message); allocation_budget=-1;
            require(std::holds_alternative<SchRequest>(result),"Fault campaign changed validation result"); success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1; failed=true; }
        require(std::get<std::string>(serialize(message))==original,"Failed parsing mutated input");
    }
    require(failed && success,"Allocation campaign missed failure/success");
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

void receive_failure_isolation() {
    LoginHub hub; std::string welcome;
    const auto source=join(hub,1,welcome); (void)join(hub,2,welcome); (void)drain(hub,1);
    const auto message=parse("BSCH "+source+" AN"+std::string(128,'x'));
    allocation_budget=0; hub.receive(1,message); allocation_budget=-1;
    require(hub.must_close(1) && hub.ready(2) && hub.output(2).empty(),"SCH allocation failure escaped or affected another peer");
    require(hub.chat_statistics()==chat::Statistics{},"SCH counted as chat traffic");
}
}
int main() {
    try { fields_and_projection(); failures_and_bounds(); ncdc_file_type_filters();
        hostile_objects_and_faults(); receive_failure_isolation();
        std::cout<<"PASS: typed SCH decoding, wire/semantic bounds, ownership and allocation isolation\n"; }
    catch (const std::exception& e) { allocation_budget=-1; std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
