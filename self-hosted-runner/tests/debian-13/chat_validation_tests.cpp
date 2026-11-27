#include "dc24h/adc/chat_validation.hpp"
#include "dc24h/adc/message_routing.hpp"

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
[[gnu::noinline]] void operator delete(void* value,std::size_t) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value,std::size_t) noexcept { std::free(value); }

namespace {
using namespace dc24h;
using namespace dc24h::adc;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
Message parse(std::string_view wire) { return std::get<Message>(parse_line(wire)); }
void add(users::SessionManager& sessions,std::uint64_t owner,std::string_view sid,users::Phase phase = users::Phase::ready) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(owner),users::Transport::adc,{})), "Add failed");
    if (phase >= users::Phase::negotiating) { require(!sessions.start_negotiation(id(owner),{}), "Negotiation failed"); }
    if (phase >= users::Phase::identified) {
        require(!sessions.identify(id(owner),*users::User::create(*users::Identity::create(sid,"C"+std::to_string(owner),"Nick"+std::to_string(owner))),{}), "Identify failed");
    }
    if (phase >= users::Phase::ready) { require(!sessions.mark_ready(id(owner),{}), "Ready failed"); }
    if (phase >= users::Phase::closing) { require(!sessions.begin_close(id(owner),users::CloseCause::shutdown,{}), "Close failed"); }
    if (phase == users::Phase::closed) { require(!sessions.finish_close(id(owner),users::CloseCause::shutdown,{}), "Finish failed"); }
}
void text_and_objects() {
    users::SessionManager sessions(3); add(sessions,1,"AAAB"); add(sessions,2,"AAAC"); chat::Core core(sessions);
    auto message = parse("BMSG AAAB Text ME0");
    for (const auto& text : std::vector<std::string>{"", "bad\ttext", "bad\rtext", std::string("a\0b",3), std::string(1,char(127)),
            std::string(1,char(0xff)), "e\xcc\x81", std::string(max_frame_bytes+1,'x')}) {
        message.parameters[0] = text;
        require(std::get<ChatValidationError>(validate_chat(core,id(1),message)) == ChatValidationError::invalid_text, "Invalid text classification differs");
        require(std::get<PublicChatError>(PublicChat(core).prepare(id(1),message)) == PublicChatError::invalid_message, "Direct public adapter bypassed text validation");
        message.type = MessageType::echo; message.target = "AAAC";
        require(std::get<PrivateChatError>(PrivateChat(core).prepare(id(1),message)) == PrivateChatError::invalid_message, "Direct private adapter bypassed text validation");
        message.type = MessageType::broadcast; message.target.reset();
    }
    for (const auto text : {" ", "  Żółw 😀  ", "\n\n", "PMBBBB ME1\nBMSG AAAC injected\\path"}) {
        message.parameters[0] = text;
        const auto view = std::get<MsgView>(validate_chat(core,id(1),message));
        require(view.text == text && view.text.data() == message.parameters[0].data() && view.third_person == false, "Valid text was normalized, trimmed or reinterpreted");
    }
    message = parse("BMSG AAAB "+std::string(max_frame_bytes-10,'x'));
    require(std::holds_alternative<MsgView>(validate_chat(core,id(1),message)), "Exact encoded frame rejected");
    message.parameters[0] += 'x';
    require(std::get<ChatValidationError>(validate_chat(core,id(1),message)) == ChatValidationError::invalid_message, "Encoded bound bypassed");
    for (int change=0; change<8; ++change) {
        auto object = parse("EMSG AAAB AAAC Valid");
        switch (change) {
        case 0: object.kind=Command::sta; break;
        case 1: object.command="STA"; break;
        case 2: object.parameters.clear(); break;
        case 3: object.positional_count=0; break;
        case 4: object.parameters.emplace_back("ME0"); object.parameters.emplace_back("ME1"); break;
        case 5: object.features={{true,"BASE"}}; break;
        case 6: object.parameters.emplace_back(std::string("XX")+char(0xff)); break;
        case 7: object.parameters.resize(max_parameters+1,"XX"); break;
        }
        require(std::holds_alternative<ChatValidationError>(validate_chat(core,id(1),object)), "Malformed complete object accepted");
    }
    auto direct = parse("DMSG AAAB AAAC Valid");
    for (const auto sid : {"", "AAA", "AAAAA", "aaab", "AAA0", "AA B", "AAAB\n"}) {
        direct.source=sid;
        require(std::get<ChatValidationError>(validate_chat(core,id(1),direct)) == ChatValidationError::invalid_sid, "Invalid source SID accepted");
        direct.source="AAAB"; direct.target=sid;
        require(std::get<ChatValidationError>(validate_chat(core,id(1),direct)) == ChatValidationError::invalid_sid, "Invalid target SID accepted");
        direct.target="AAAC";
    }
    direct.source.reset(); require(std::get<ChatValidationError>(validate_chat(core,id(1),direct)) == ChatValidationError::invalid_sid, "Missing source dereferenced");
    direct.source="AAAB"; direct.target.reset();
    require(std::get<ChatValidationError>(validate_chat(core,id(1),direct)) == ChatValidationError::invalid_sid, "Missing target dereferenced");
    direct.target="AAAA";
    require(std::holds_alternative<MsgView>(validate_chat(core,id(1),direct)) &&
        std::get<PrivateChatError>(PrivateChat(core).prepare(id(1),direct)) == PrivateChatError::recipient_unavailable, "Syntactically valid missing target confused with ownership");
    direct.target="AAAC"; direct.source="AAAA";
    require(std::get<ChatValidationError>(validate_chat(core,id(1),direct)) == ChatValidationError::invalid_sid, "Reserved hub SID used as client source");
}
void states_and_revalidation() {
    for (const auto phase : {users::Phase::accepted,users::Phase::negotiating,users::Phase::identified,users::Phase::ready,users::Phase::closing,users::Phase::closed}) {
        users::SessionManager sessions(3); add(sessions,1,"AAAB",phase); add(sessions,2,"AAAC"); chat::Core core(sessions);
        const auto message=parse("EMSG AAAB AAAC StateSecret PMAAAB");
        const auto result=validate_chat(core,id(1),message);
        if (phase == users::Phase::ready) { require(std::holds_alternative<MsgView>(result), "Ready sender refused"); }
        else {
            require(std::get<ChatValidationError>(result) == ChatValidationError::invalid_state &&
                std::get<PrivateChatError>(PrivateChat(core).prepare(id(1),message)) == PrivateChatError::invalid_state &&
                std::get<RoutingError>(MessageRouter(core).prepare(id(1),message)) == RoutingError::invalid_state, "Non-ready sender bypassed shared gate");
        }
        require(std::get<ChatValidationError>(validate_chat(core,id(99),message)) == ChatValidationError::invalid_state, "Missing sender accepted");
    }
    for (const auto phase : {users::Phase::accepted,users::Phase::negotiating,users::Phase::identified,users::Phase::closing,users::Phase::closed}) {
        users::SessionManager sessions(2); add(sessions,1,"AAAB"); add(sessions,2,"AAAC",phase); chat::Core core(sessions);
        const auto message=parse("EMSG AAAB AAAC TargetSecret PMAAAB");
        require(std::get<PrivateChatError>(PrivateChat(core).prepare(id(1),message)) == PrivateChatError::recipient_unavailable, "Non-ready target received private chat");
    }
    users::SessionManager sessions(2); add(sessions,1,"AAAB"); add(sessions,2,"AAAC"); chat::Core core(sessions);
    const auto message=parse("BMSG AAAB ValidatedSecret");
    require(std::get<ChatValidationError>(validate_chat(core,id(2),message)) == ChatValidationError::wrong_source, "Source not bound to connection");
    require(std::get<ChatValidationError>(validate_chat(core,id(1),parse("FMSG AAAB +TCP4 Text"))) == ChatValidationError::unsupported_type &&
        std::get<ChatValidationError>(validate_chat(core,id(2),parse("FMSG AAAB +TCP4 Text"))) == ChatValidationError::wrong_source, "Feature source gate order changed");
    for (const auto wire : {"HMSG Text", "IMSG Text"}) {
        require(std::get<ChatValidationError>(validate_chat(core,id(1),parse(wire))) == ChatValidationError::unsupported_type, "Hub scope became client chat");
    }
    const auto before=sessions.statistics(); bool failed=false, success=false;
    for (long budget=0; budget<100 && !success; ++budget) {
        allocation_budget=budget;
        try { const auto result=validate_chat(core,id(1),message); allocation_budget=-1; require(std::holds_alternative<MsgView>(result), "Fault campaign typed error"); success=true; }
        catch (const std::bad_alloc&) { allocation_budget=-1; failed=true; }
        require(sessions.statistics() == before, "Validation allocated partial session changes");
    }
    require(failed && success, "Validation allocation campaign incomplete");
    const auto view=std::get<MsgView>(validate_chat(core,id(1),message));
    require(!sessions.rename(id(1),"Renamed",{}) && std::holds_alternative<MsgView>(validate_chat(core,id(1),message)), "Rename changed source binding");
    const auto plan=std::get<PublicBroadcast>(PublicChat(core).prepare(id(1),message));
    require(!sessions.begin_close(id(1),users::CloseCause::shutdown,{}), "Close failed");
    require(view.text == "ValidatedSecret" && std::get<PublicChatError>(PublicChat(core).prepare(id(1),message)) == PublicChatError::invalid_state, "Borrowed view acted as cached permission");
    unsigned calls=0;
    const auto report=PublicChat(core).dispatch(plan,[&](users::SessionId,const SharedPublicChatFrame&) { ++calls; return true; });
    require(std::get<chat::Error>(report) == chat::Error::sender_not_ready && calls == 0, "Dispatch did not revalidate accepted sender");
}
} // namespace
int main() {
    try { text_and_objects(); states_and_revalidation(); std::cout << "PASS: shared chat text/SID/session gates, direct adapters, target readiness and dispatch revalidation\n"; }
    catch (const std::exception& error) { allocation_budget=-1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
