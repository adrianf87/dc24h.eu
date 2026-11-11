#include "dc24h/adc/message_routing.hpp"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { long allocation_budget=-1; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (allocation_budget==0) { throw std::bad_alloc(); }
    if (allocation_budget>0) { --allocation_budget; }
    if (auto* p=std::malloc(size==0?1:size)) { return p; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
namespace {
using namespace dc24h;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
adc::Message parse(std::string_view line) { return std::get<adc::Message>(adc::parse_line(line)); }
void add(users::SessionManager& sessions,std::uint64_t value,std::string_view sid,bool ready=true) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(value),users::Transport::adc,{})),"Add failed");
    require(!sessions.start_negotiation(id(value),{}) &&
        !sessions.identify(id(value),*users::User::create(*users::Identity::create(sid,"CID"+std::to_string(value),"Nick"+std::to_string(value))),{}),"Identify failed");
    if (ready) { require(!sessions.mark_ready(id(value),{}),"Ready failed"); }
}
adc::RoutedMessage prepare(const adc::MessageRouter& router,std::string_view line) {
    auto result=router.prepare(id(1),parse(line));require(std::holds_alternative<adc::RoutedMessage>(result),"Preparation failed");
    return std::get<adc::RoutedMessage>(std::move(result));
}
void policy_and_projection() {
    users::SessionManager sessions(8);add(sessions,1,"AAAB");add(sessions,2,"AAAC");add(sessions,3,"AAAD");add(sessions,4,"AAAE",false);
    chat::Core core(sessions);adc::MessageRouter router(core);
    struct Case { const char* input;const char* wire;std::array<std::uint64_t,3> recipients;std::size_t count; };
    for (const auto& item:std::array{
            Case{"BMSG AAAB Hi\\sŻółw\\n\\\\ ME01 XXsecret PDsecret","BMSG AAAB Hi\\sŻółw\\n\\\\ ME1\n",{1,2,3},3},
            Case{"DMSG AAAB AAAC Secret PMAAAB XXsecret","DMSG AAAB AAAC Secret PMAAAB\n",{2,0,0},1},
            Case{"EMSG AAAB AAAC Secret PMAAAB ME0","EMSG AAAB AAAC Secret PMAAAB ME0\n",{2,1,0},2},
            Case{"EMSG AAAB AAAB Self PMAAAB","EMSG AAAB AAAB Self PMAAAB\n",{1,0,0},1},
            Case{"DMSG AAAB AAAB Self","DMSG AAAB AAAB Self\n",{1,0,0},1}}) {
        const auto plan=prepare(router,item.input);
        std::array<std::uint64_t,3> recipients{};std::size_t count=0;bool safe=true;const void* first_packet=nullptr;
        const adc::MessageRouter::Sink sink=[&](users::SessionId recipient,const adc::RoutedFrame& frame) {
            recipients.at(count++)=recipient.value();
            std::visit([&](const auto& packet) {
                safe=safe && packet && packet->wire()==item.wire;
                if (first_packet) { safe=safe && first_packet==packet.get(); }
                first_packet=packet.get();
            },frame);return true;
        };
        allocation_budget=0;const auto result=router.dispatch(plan,sink);allocation_budget=-1;
        require(safe && count==item.count && recipients==item.recipients &&
            std::get<chat::DispatchReport>(result)==chat::DispatchReport{item.count,0,0},"Route scope, canonical projection, packet identity or allocation-free dispatch failed");
        require(std::get<chat::Error>(router.dispatch(plan,{}))==chat::Error::missing_sink,"Empty sink accepted");
    }
    for (const auto& [line,error]:std::array{
            std::pair{"BMSG AAAB Hidden PMAAAB",adc::RoutingError::private_broadcast},
            std::pair{"EMSG AAAB AAAC Hidden PMAAAD",adc::RoutingError::unsupported_group},
            std::pair{"FMSG AAAB +TCP4 Hidden",adc::RoutingError::unsupported_type},
            std::pair{"HMSG Hidden",adc::RoutingError::unsupported_type},
            std::pair{"IMSG Hidden",adc::RoutingError::unsupported_type},
            std::pair{"DMSG AAAB ZZZZ Hidden",adc::RoutingError::recipient_unavailable},
            std::pair{"EMSG AAAB AAAE Hidden",adc::RoutingError::recipient_unavailable},
            std::pair{"BMSG AAAC Forged",adc::RoutingError::wrong_source},
            std::pair{"FMSG AAAC +TCP4 Forged",adc::RoutingError::wrong_source},
            std::pair{"DMSG AAAB AAAC Hidden PMAAAB ME2",adc::RoutingError::invalid_message}}) {
        require(std::get<adc::RoutingError>(router.prepare(id(1),parse(line)))==error,"Unsafe routing policy accepted");
    }
    auto malformed=parse("DMSG AAAB AAAC Hidden");malformed.target.reset();
    require(std::get<adc::RoutingError>(router.prepare(id(1),malformed))==adc::RoutingError::invalid_message,"Malformed object dereferenced/routed");
    require(std::get<adc::RoutingError>(router.prepare(id(99),parse("BMSG AAAB Hidden")))==adc::RoutingError::invalid_state,"Missing sender accepted");
    require(std::get<adc::RoutingError>(router.prepare(id(4),parse("BMSG AAAE Hidden")))==adc::RoutingError::invalid_state,"Welcome-pending sender accepted");
}
void lifetime_and_domain() {
    users::SessionManager sessions(8),other_sessions(8);
    for (auto* registry:{&sessions,&other_sessions}) { add(*registry,1,"AAAB");add(*registry,2,"AAAC");add(*registry,3,"AAAD"); }
    alignas(chat::Core) std::byte engine_storage[sizeof(chat::Core)];
    const auto destroy=[](chat::Core* value) { std::destroy_at(value); };
    std::unique_ptr<chat::Core,decltype(destroy)> engine(
        std::construct_at(reinterpret_cast<chat::Core*>(engine_storage),sessions),destroy);
    adc::MessageRouter router(*engine);
    chat::Core other_core(other_sessions);adc::MessageRouter other(other_core);
    const auto public_plan=prepare(router,"BMSG AAAB Public");
    const auto private_plan=prepare(router,"EMSG AAAB AAAC Secret PMAAAB");
    unsigned calls=0;const adc::MessageRouter::Sink sink=[&](users::SessionId,const adc::RoutedFrame&) { ++calls;return true; };
    for (const auto* plan:{&public_plan,&private_plan}) {
        require(std::get<chat::Error>(other.dispatch(*plan,sink))==chat::Error::foreign_plan && calls==0,"Foreign engine accepted a plan with colliding IDs/SIDs");
    }
    require(!sessions.finish_close(id(2),users::CloseCause::peer_closed,{}) && !sessions.remove(id(2)),"Remove failed");
    add(sessions,5,"AAAC");
    require(std::get<chat::Error>(router.dispatch(private_plan,sink))==chat::Error::recipient_unavailable && calls==0,"Stale target reached replacement or emitted echo");
    require(std::get<chat::DispatchReport>(router.dispatch(public_plan,sink))==chat::DispatchReport{2,1,0} && calls==2,"Public plan reached later join");
    calls=0;
    // Deliberately reconstruct Core at exactly the same address. The token is
    // retained by old plans, so pointer-address equality cannot authorize them.
    auto* old_address=engine.get();engine.reset();engine.reset(std::construct_at(old_address,sessions));
    require(old_address==&*engine,"Fixture failed to reuse Core storage");adc::MessageRouter replacement(*engine);
    for (const auto* plan:{&public_plan,&private_plan}) {
        require(std::get<chat::Error>(replacement.dispatch(*plan,sink))==chat::Error::foreign_plan && calls==0,"Reconstructed engine accepted an old domain");
    }
    auto current=prepare(replacement,"EMSG AAAB AAAC Current PMAAAB");auto moved=std::move(current);
    require(std::get<chat::Error>(replacement.dispatch(current,sink))==chat::Error::invalid_text && calls==0,"Moved-from plan delivered");
    require(std::get<chat::DispatchReport>(replacement.dispatch(moved,sink)).queued==2 && calls==2,"Fresh domain could not route");
    require(!sessions.begin_close(id(1),users::CloseCause::shutdown,{}),"Close failed");calls=0;
    require(std::get<chat::Error>(replacement.dispatch(moved,sink))==chat::Error::sender_not_ready && calls==0,"Closing sender routed");
}
void callback_changes_and_faults() {
    users::SessionManager sessions(8);add(sessions,1,"AAAB");add(sessions,2,"AAAC");add(sessions,3,"AAAD");
    chat::Core core(sessions);adc::MessageRouter router(core);
    const auto input=parse("BMSG AAAB "+std::string(160,'x')+" ME1 XXprivate");
    const auto before=sessions.statistics();bool failed=false,success=false;
    for (long budget=0;budget<120 && !success;++budget) {
        allocation_budget=budget;
        try { const auto result=router.prepare(id(1),input);allocation_budget=-1;require(std::holds_alternative<adc::RoutedMessage>(result),"Preparation typed error");success=true; }
        catch (const std::bad_alloc&) { allocation_budget=-1;failed=true; }
        require(sessions.statistics()==before,"Preparation failure changed registry");
    }
    require(failed && success,"Preparation fault sweep missed boundary");
    const auto plan=prepare(router,"BMSG AAAB Publication");std::vector<std::uint64_t> calls;
    const auto result=router.dispatch(plan,[&](users::SessionId recipient,const adc::RoutedFrame&) {
        calls.push_back(recipient.value());
        if (recipient==id(1)) {
            require(!sessions.force_close(id(1),users::CloseCause::output_limit,{}),"Sender close failed");
            require(!sessions.finish_close(id(2),users::CloseCause::peer_closed,{}) && !sessions.remove(id(2)),"Target cleanup failed");
            add(sessions,4,"AAAC");return false;
        }
        return true;
    });
    require(std::get<chat::DispatchReport>(result)==chat::DispatchReport{1,1,1} && calls==std::vector<std::uint64_t>{1,3},"Callback mutation invalidated traversal, retried or reached replacement");
}
}
int main() {
    try { policy_and_projection();lifetime_and_domain();callback_changes_and_faults();std::cout<<"PASS: Message Routing policy, routing domains, lifetime isolation, shared frames and preparation faults\n"; }
    catch (const std::exception& error) { allocation_budget=-1;std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
