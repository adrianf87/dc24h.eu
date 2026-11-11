#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <type_traits>

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
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
Message parse(std::string_view wire) { return std::get<Message>(parse_line(wire)); }
void add(users::SessionManager& sessions, std::uint64_t number, bool ready = true) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(number),users::Transport::adc,{})), "Add failed");
    require(!sessions.start_negotiation(id(number),{}) && !sessions.identify(id(number),
        *users::User::create(*users::Identity::create("S"+std::to_string(number),"C"+std::to_string(number),"Nick"+std::to_string(number))),{}), "Identify failed");
    if (ready) { require(!sessions.mark_ready(id(number),{}), "Ready failed"); }
}
std::string drain(LoginHub& hub, std::uint64_t owner) {
    std::string result;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) {
        const std::string frame(hub.output(owner)); result += frame; hub.consume(owner,frame.size());
    }
    return result;
}
std::string join(LoginHub& hub, std::uint64_t owner, bool complete = true) {
    hub.connect(owner,"127.0.0.1"); hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size()); const std::string sid(hub.output(owner).substr(5,4));
    hub.consume(owner,hub.output(owner).size()); TigerDigest pid{}; pid[0] = static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)));
    if (complete) {
        const auto welcome = drain(hub,owner);
        require(hub.ready(owner) && welcome.ends_with(std::get<SharedHubMessageFrame>(make_hub_message(hub_welcome_text))->wire()), "Welcome IMSG missing or incorrectly ordered");
    }
    return sid;
}

void codec_and_domains() {
    static_assert(!std::is_copy_constructible_v<HubMessageFrame> && !std::is_move_constructible_v<HubMessageFrame>);
    const auto frame = std::get<SharedHubMessageFrame>(make_hub_message("System Żółw\nBMSG AAAB forged\\path PMAAAB ME1"));
    require(frame->wire() == "IMSG System\\sŻółw\\nBMSG\\sAAAB\\sforged\\\\path\\sPMAAAB\\sME1\n" &&
        !frame->message().source && !frame->message().target && frame->message().parameters.size() == 1 &&
        !std::get<MsgView>(validate_msg(frame->message())).private_group, "Hub frame exposed identity/flags or command injection");
    for (const auto text : {"", "bad\rtext", "e\xcc\x81", "\xff"}) {
        require(std::get<HubMessageError>(make_hub_message(text)) == HubMessageError::invalid_text, "Invalid system text accepted");
    }
    require(std::get<SharedHubMessageFrame>(make_hub_message(std::string(max_frame_bytes-5,'x')))->wire().size() == max_frame_bytes+1, "Exact IMSG limit failed");
    require(std::get<HubMessageError>(make_hub_message(std::string(max_frame_bytes-4,'x'))) == HubMessageError::frame_too_long, "Oversized frame accepted");
    const auto escaped = std::string((max_frame_bytes-6)/2,' ')+'x';
    require(std::get<SharedHubMessageFrame>(make_hub_message(escaped))->wire().size() == max_frame_bytes+1 &&
        std::get<HubMessageError>(make_hub_message(escaped+' ')) == HubMessageError::frame_too_long, "Escape expansion not bounded");
    users::SessionManager sessions(300), other_sessions(300);
    for (std::uint64_t number=1; number<=270; ++number) { add(sessions,number); }
    add(sessions,271,false); add(other_sessions,1);
    HubMessages messages(sessions), other(other_sessions), same_registry(sessions);
    const auto plan = std::get<HubDelivery>(messages.prepare("System snapshot"));
    require(plan.recipients().size() == 270, "Hub snapshot truncated at page boundary/included pending");
    unsigned calls = 0; const HubMessageFrame* packet = nullptr;
    const HubMessages::Sink sink = [&](users::SessionId, const SharedHubMessageFrame& value) {
        ++calls; if (packet) { require(packet == value.get(), "Packet copied per recipient"); } packet=value.get(); return true;
    };
    allocation_budget=0; const auto result=messages.dispatch(plan,sink); allocation_budget=-1;
    require(std::get<chat::DispatchReport>(result) == chat::DispatchReport{270,0,0} && calls == 270, "Dispatch allocated or miscounted");
    calls=0;
    for (const auto* service : {&other,&same_registry}) {
        require(std::get<HubMessageError>(service->dispatch(plan,sink)) == HubMessageError::foreign_plan && calls == 0, "Foreign hub plan delivered");
    }
    require(std::get<HubMessageError>(messages.dispatch(plan,{})) == HubMessageError::missing_sink, "Empty sink accepted");
    require(std::get<HubMessageError>(messages.prepare("x",id(271))) == HubMessageError::recipient_unavailable &&
        std::get<HubMessageError>(messages.prepare("x",id(299))) == HubMessageError::recipient_unavailable, "Pending/missing target accepted");
    auto targeted=std::get<HubDelivery>(messages.prepare("Secret",id(2))); auto moved=std::move(targeted);
    require(std::get<HubMessageError>(messages.dispatch(targeted,sink)) == HubMessageError::invalid_plan, "Moved plan delivered");
    require(!sessions.finish_close(id(2),users::CloseCause::peer_closed,{}) && !sessions.remove(id(2)), "Close failed");
    add(sessions,272); // New lifetime must not inherit a snapshot or target.
    require(std::get<chat::DispatchReport>(messages.dispatch(moved,sink)) == chat::DispatchReport{0,1,0} && calls == 0, "Stale target received IMSG");
    const auto report=messages.dispatch(plan,[&](users::SessionId owner,const SharedHubMessageFrame&) {
        if (owner == id(1)) { (void)sessions.force_close(id(3),users::CloseCause::shutdown,{}); return false; }
        return true;
    });
    require(std::get<chat::DispatchReport>(report) == chat::DispatchReport{267,2,1}, "Callback mutation/refusal/late join broke snapshot");
    const auto before=sessions.statistics(); bool failed=false, succeeded=false;
    for (long budget=0; budget<256 && !succeeded; ++budget) {
        allocation_budget=budget;
        try { const auto prepared=messages.prepare(std::string_view("System preparation with allocation rollback")); allocation_budget=-1;
            require(std::holds_alternative<HubDelivery>(prepared), "Unexpected preparation error"); succeeded=true; }
        catch (const std::bad_alloc&) { allocation_budget=-1; failed=true; }
        require(sessions.statistics() == before, "Failed preparation mutated sessions");
    }
    require(failed && succeeded, "Allocation campaign did not complete");
}

void queues_and_welcome() {
    LoginHub boundary; const auto own=join(boundary,1,false);
    while (!boundary.output(1).starts_with("BINF "+own+" ")) { boundary.consume(1,boundary.output(1).size()); }
    boundary.consume(1,boundary.output(1).size()-1);
    require(!boundary.ready(1), "Partial own INF became ready");
    require(boundary.consume(1,1) && boundary.ready(1) && boundary.output(1).starts_with("IMSG Welcome"), "Greeting changed final-LF readiness");
    require(!boundary.consume(1,1) && boundary.ready(1), "Partial IMSG duplicated readiness");
    require(!boundary.consume(1,boundary.output(1).size()) && boundary.ready(1), "Complete IMSG duplicated readiness");
    boundary.disconnect(1,false);
    LoginHub hub; const auto first=join(hub,1); join(hub,2); (void)drain(hub,1); join(hub,3,false); (void)drain(hub,1); (void)drain(hub,2);
    const auto before=hub.queued_bytes();
    require(std::get<HubMessageError>(hub.send_hub_message("x",id(3))) == HubMessageError::recipient_unavailable && hub.queued_bytes() == before, "Pending target changed queues");
    require(std::get<chat::DispatchReport>(hub.send_hub_message("SystemShared Żółw\n\\path")) == chat::DispatchReport{2,0,0}, "Hub broadcast recipients differ");
    const auto pinned=hub.output(1); const auto* pointer=pinned.data();
    require(hub.output(2).data() == pointer, "Hub queue packets not shared");
    hub.consume(1,3);
    require(std::get<chat::DispatchReport>(hub.send_hub_message("TargetOnly",id(1))).queued == 1 && hub.output(1).data() == pointer+3, "Target append moved pinned front");
    (void)drain(hub,2); hub.disconnect(2);
    require(hub.output(1) == pinned.substr(3), "Recipient cleanup invalidated pinned packet");
    const std::string remaining(pinned.substr(3));
    hub.close(1,CloseReason::idle_timeout); const auto terminal=drain(hub,1);
    require(terminal.starts_with(remaining) && terminal.find("TargetOnly") == std::string::npos &&
        terminal.find("ISTA 230") != std::string::npos && terminal.find("IQUI "+first) != std::string::npos, "Terminal close broke pinned system output or canceled tail");
    hub.disconnect(1,false); hub.disconnect(3,false); require(hub.queued_bytes() == 0, "System queue charges leaked");
    LoginLimits limits; limits.output_per_client=512; limits.output_total=4096; LoginHub capped(limits);
    join(capped,1); join(capped,2); (void)drain(capped,1);
    require(std::get<chat::DispatchReport>(capped.send_hub_message(std::string(470,'x'),id(1))).queued == 1, "Fill failed");
    const auto front=capped.output(1); const auto* address=front.data();
    require(std::get<chat::DispatchReport>(capped.send_hub_message(std::string(40,'y'))) == chat::DispatchReport{1,0,1} &&
        capped.overloaded(1) && capped.output(1).data() == address && capped.output(1) == front && capped.ready(2), "Slow system recipient not isolated");
    require(drain(capped,2) == "IMSG "+std::string(40,'y')+"\n", "Healthy system recipient lost content");
    capped.disconnect(1,false); capped.disconnect(2,false); require(capped.queued_bytes() == 0, "Cap cleanup leaked charges");
    LoginLimits global; global.output_per_client=512; global.output_total=512; LoginHub aggregate(global);
    join(aggregate,1); join(aggregate,2); (void)drain(aggregate,1);
    require(std::get<chat::DispatchReport>(aggregate.send_hub_message(std::string(200,'x'))) == chat::DispatchReport{2,0,0} &&
        aggregate.queued_bytes() == 412, "Shared system packets did not charge each recipient");
    require(std::get<chat::DispatchReport>(aggregate.send_hub_message(std::string(100,'y'),id(1))) == chat::DispatchReport{0,0,1} &&
        aggregate.overloaded(1) && aggregate.ready(2), "Global system output limit not enforced");
    aggregate.disconnect(1,false); require(aggregate.queued_bytes() == 206, "Global cancellation charge differs");
    (void)drain(aggregate,2); aggregate.disconnect(2,false); require(aggregate.queued_bytes() == 0, "Global cleanup leaked charges");
}
} // namespace
int main() {
    try { codec_and_domains(); queues_and_welcome(); std::cout << "PASS: IMSG codec, trusted selection, domains, lifetimes, welcome and bounded shared queues\n"; }
    catch (const std::exception& error) { allocation_budget=-1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
