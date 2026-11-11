#include "dc24h/search/core.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
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
using namespace dc24h::search;
void require(bool value, const char* why) { if (!value) { throw std::runtime_error(why); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
QueryData input() {
    QueryData data;
    data.include = {"Ubuntu", "Żółw 😀"}; data.exclude = {"Beta"}; data.extensions = {"iso", "IMG"};
    data.min_bytes = 0; data.max_bytes = INT64_MAX; data.exact_bytes = 4096;
    data.type = ItemType::file; data.token = "opaque-Token\\1";
    return data;
}
void query_validation() {
    const auto data = input(); const auto query = std::get<Query>(Query::create(data));
    require(query.data() == data, "Checked query altered terms, size, type, case or token");
    for (const auto value : {ItemType::any, ItemType::file, ItemType::directory}) {
        auto candidate = data; candidate.type = value;
        require(std::holds_alternative<Query>(Query::create(candidate)), "Valid item type refused");
    }
    auto bad = data; bad.include.clear();
    require(std::get<QueryError>(Query::create(bad)) == QueryError::missing_term, "Empty positive criteria accepted");
    for (const auto& text : std::vector<std::string>{"", "   ", "bad\ntext", "bad\rtext", "bad\ttext", std::string("bad\0text",8),
        "e\xcc\x81", std::string(1, char(0xff)), "bad\xe2\x80\xae", std::string(max_term_bytes+1, 'x')}) {
        bad = data; bad.include = {text};
        require(std::get<QueryError>(Query::create(bad)) == QueryError::invalid_text, "Invalid positive text accepted");
        bad = data; bad.exclude = {text};
        require(std::get<QueryError>(Query::create(bad)) == QueryError::invalid_text, "Invalid exclusion accepted");
    }
    bad = data; bad.include = {"  spaced * literal?  "}; bad.exclude = {"Path\\Name"};
    require(std::get<Query>(Query::create(bad)).data() == bad, "Whitespace trimmed or wildcard interpreted");
    bad = {}; bad.include = std::vector<std::string>(max_terms, "x");
    require(std::holds_alternative<Query>(Query::create(bad)), "Exact term count rejected");
    bad.exclude = {"y"};
    require(std::get<QueryError>(Query::create(bad)) == QueryError::too_many_terms, "Combined term budget bypassed");
    bad = {}; bad.include = std::vector<std::string>(16, std::string(max_term_bytes, 'x'));
    require(std::holds_alternative<Query>(Query::create(bad)), "Exact aggregate byte bound rejected");
    bad.token = "x";
    require(std::get<QueryError>(Query::create(bad)) == QueryError::too_large, "Token not charged to byte bound");
    for (const auto& extension : std::vector<std::string>{"", "/iso", "a\\b", "two words", "bad\t", "e\xcc\x81", std::string(max_extension_bytes+1,'x')}) {
        bad = data; bad.extensions = {extension};
        require(std::get<QueryError>(Query::create(bad)) == QueryError::invalid_extension, "Invalid extension accepted");
    }
    bad = data; bad.extensions = std::vector<std::string>(max_extensions+1,"iso");
    require(std::get<QueryError>(Query::create(bad)) == QueryError::too_many_terms, "Extension count not bounded");
    for (const auto& token : std::vector<std::string>{"", "two words", "bad\n", "Żółw", std::string(max_token_bytes+1,'x')}) {
        bad = data; bad.token = token;
        require(std::get<QueryError>(Query::create(bad)) == QueryError::invalid_token, "Invalid opaque token accepted");
    }
    bad = data; bad.token = std::string(max_token_bytes, '~');
    require(std::holds_alternative<Query>(Query::create(bad)), "Exact token bound rejected");
    bad = data; bad.type = static_cast<ItemType>(99);
    require(std::get<QueryError>(Query::create(bad)) == QueryError::invalid_type, "Invalid enum accepted");
    for (int which = 0; which < 3; ++which) {
        bad = data;
        if (which == 0) { bad.min_bytes = -1; } else if (which == 1) { bad.max_bytes = -1; } else { bad.exact_bytes = -1; }
        require(std::get<QueryError>(Query::create(bad)) == QueryError::invalid_size, "Negative size accepted");
    }
    for (const auto sizes : {std::array<std::int64_t,3>{5,4,4}, {1,4,5}, {2,4,1}}) {
        bad = data; bad.min_bytes=sizes[0]; bad.max_bytes=sizes[1]; bad.exact_bytes=sizes[2];
        require(std::get<QueryError>(Query::create(bad)) == QueryError::invalid_size, "Contradictory sizes accepted");
    }
    bad = data; bad.min_bytes = INT64_MAX; bad.max_bytes = INT64_MAX; bad.exact_bytes = INT64_MAX;
    require(std::holds_alternative<Query>(Query::create(bad)), "Signed integer upper bound rejected");
    bad = {}; bad.include = {"x"};
    require(std::holds_alternative<Query>(Query::create(bad)), "Optional fields became required");
}
void add(users::SessionManager& sessions, std::uint64_t value, int phase = 3,
         std::vector<std::string> features = {}, std::string source = {}) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(value), users::Transport::adc, {})), "Add failed");
    if (phase < 1) { return; }
    require(!sessions.start_negotiation(id(value), {}), "Negotiation failed");
    if (phase < 2) { return; }
    users::ProfileData profile; profile.features = std::move(features);
    const auto suffix = std::to_string(value);
    const auto user = *users::User::create(*users::Identity::create(source.empty() ? "SID"+suffix : source, "CID"+suffix, "Nick"+suffix),
                                        *users::Profile::create(std::move(profile)));
    require(!sessions.identify(id(value), user, {}), "Identify failed");
    if (phase >= 3) { require(!sessions.mark_ready(id(value), {}), "Ready failed"); }
}
Plan prepare(const Core& core, const Selection& selection = {}) {
    auto result = core.prepare(id(1), "SID1", input(), selection);
    require(std::holds_alternative<Plan>(result), "Search preparation failed");
    return std::get<Plan>(std::move(result));
}
void close(users::SessionManager& sessions, std::uint64_t value) {
    require(!sessions.finish_close(id(value),users::CloseCause::peer_closed,{}) && !sessions.remove(id(value)), "Cleanup failed");
}
void state_and_ownership() {
    users::SessionManager sessions(8); Core core(sessions);
    require(std::get<Error>(core.prepare(id(1),"SID1",{})) == Error::missing_sender, "Missing sender accepted");
    add(sessions,1,0);
    require(std::get<Error>(core.prepare(id(1),"SID1",input())) == Error::sender_not_ready, "Accepted sender used");
    require(!sessions.start_negotiation(id(1),{}), "Start failed");
    require(std::get<Error>(core.prepare(id(1),"SID1",input())) == Error::sender_not_ready, "Negotiating sender used");
    require(!sessions.identify(id(1),*users::User::create(*users::Identity::create("SID1","CID1","Nick1")),{}), "Identify failed");
    require(std::get<Error>(core.prepare(id(1),"SID1",input())) == Error::sender_not_ready, "Welcome-pending sender used");
    require(!sessions.mark_ready(id(1),{}), "Ready failed"); add(sessions,2);
    require(std::get<Error>(core.prepare(id(1),"SID2",{})) == Error::wrong_source, "Source check did not precede query work");
    auto data=input(); Selection selected; std::string source="SID1";
    auto result=core.prepare(id(1),source,data,selected); auto plan=std::get<Plan>(std::move(result));
    source="other"; data.include[0]="mutated"; data.token="mutated"; selected.excluded_features={"BASE"};
    require(plan.query().data()==input() && plan.source()=="SID1" && plan.selection()==Selection{}, "Plan borrowed mutable input");
    Core other(sessions); unsigned attempts=0;
    const Core::Sink sink=[&](users::SessionId,const Plan&) { ++attempts; return true; };
    require(std::get<Error>(other.dispatch(plan,sink)) == Error::foreign_plan && attempts==0, "Foreign Core accepted plan");
    require(std::get<Error>(core.dispatch(plan,{})) == Error::missing_sink, "Empty sink accepted");
    require(!sessions.rename(id(1),"Renamed",{}), "Rename failed");
    require(std::get<DispatchReport>(core.dispatch(plan,sink)) == DispatchReport{2,0,0,0}, "Rename invalidated stable SID");
    auto copied=plan; close(sessions,1);
    add(sessions,3,3,{},"SID1");
    require(std::get<Error>(core.dispatch(copied,sink)) == Error::missing_sender && attempts==2, "Reused SID replaced lifetime");
    require(plan.query().data()==input(), "Plan depended on removed sender storage");
    require(std::get<Error>(core.prepare(id(3),"SID1",input(),Selection{{"TCP4"},{"TCP4"}})) == Error::invalid_selection, "Contradictory selectors accepted");
}
void recipients_and_features() {
    users::SessionManager sessions(400); add(sessions,1,3,{"BASE","TCP4"}); Core core(sessions);
    for (std::uint64_t value=2; value<=270; ++value) { add(sessions,value,3,value%2==0 ? std::vector<std::string>{"BASE","TCP4"} : std::vector<std::string>{"BASE","TCP6"}); }
    add(sessions,271,0); add(sessions,272,1); add(sessions,273,2); add(sessions,274);
    require(!sessions.begin_close(id(274),users::CloseCause::shutdown,{}), "Close failed");
    auto all=prepare(core);
    require(all.recipients().size()==270 && all.recipients().front()==id(1) && all.recipients().back()==id(270), "Multipage selection includes unready or skips ready source");
    Selection selected{{"BASE","TCP4"},{"TCP6"}}; const auto plan=prepare(core,selected);
    require(plan.recipients().size()==136 && plan.recipients()[1]==id(2), "Feature inclusion/exclusion differs");
    for (const auto& invalid : std::vector<Selection>{{{"tcp4"},{}},{{"TC"},{}},{{"TCP44"},{}},{{"TCP4","TCP4"},{}},{{},{"TCP4","TCP4"}},{{"TCP4"},{"TCP4"}},
                                                     {std::vector<std::string>(max_selectors+1,"BASE"),{}}}) {
        require(std::get<Error>(core.prepare(id(1),"SID1",input(),invalid)) == Error::invalid_selection, "Invalid selector accepted");
    }
    const auto empty=prepare(core,{{"ZZZZ"},{}});
    require(empty.recipients().empty() && std::get<DispatchReport>(core.dispatch(empty,[](auto,const auto&) { return true; }))==DispatchReport{}, "Zero-recipient plan incorrectly rejected");
    close(sessions,2); add(sessions,275,3,{"BASE","TCP4"},"SID2");
    users::ProfileData new_profile; new_profile.features={"BASE","TCP6"};
    const auto* old=sessions.find(id(4));
    require(!sessions.update_user(id(4),*users::User::create(*old->identity(),*users::Profile::create(new_profile)),{}), "Feature update failed");
    std::vector<std::uint64_t> delivered;
    const auto result=core.dispatch(plan,[&](users::SessionId recipient,const Plan& body) {
        require(body.query().data()==input(), "Dispatch changed query"); delivered.push_back(recipient.value()); return recipient!=id(6);
    });
    require(std::get<DispatchReport>(result)==DispatchReport{133,2,1,0} && delivered.size()==134 &&
            std::find(delivered.begin(),delivered.end(),275)==delivered.end(), "Stale feature/lifetime or late join selection differs");
}
void cancellation_and_domains() {
    users::SessionManager sessions(4); for (std::uint64_t i=1;i<=3;++i) { add(sessions,i); }
    Core core(sessions); const auto plan=prepare(core);
    std::size_t calls=0;
    const Core::Sink close_sender=[&](users::SessionId,const Plan&) {
        ++calls; require(!sessions.begin_close(id(1),users::CloseCause::peer_closed,{}), "Sink close failed"); return true;
    };
    require(std::get<DispatchReport>(core.dispatch(plan,close_sender))==DispatchReport{1,0,0,2} && calls==1, "Sender closure continued fanout");
    require(std::get<Error>(core.dispatch(plan,close_sender))==Error::sender_not_ready, "Already-closing source dispatched");
    users::SessionManager fresh(1); add(fresh,1);
    alignas(Core) unsigned char storage[sizeof(Core)];
    auto* first=::new(static_cast<void*>(storage)) Core(fresh); const auto old=prepare(*first); first->~Core();
    auto* second=::new(static_cast<void*>(storage)) Core(fresh);
    require(std::get<Error>(second->dispatch(old,[](auto,const auto&) { return true; }))==Error::foreign_plan, "Reused Core address accepted old domain");
    second->~Core();
}
void fault_campaign() {
    users::SessionManager sessions(300); add(sessions,1); add(sessions,2); Core core(sessions);
    auto data=input(); data.include={std::string(128,'x')}; data.token=std::string(64,'t');
    const Selection selection{{},{"TCP6"}}; const auto before=sessions.statistics(); bool failed=false, success=false;
    for (long budget=0; budget<100 && !success; ++budget) {
        allocation_budget=budget;
        try { const auto plan=core.prepare(id(1),"SID1",data,selection); allocation_budget=-1;
            require(std::holds_alternative<Plan>(plan), "Allocation campaign returned validation failure"); success=true;
        } catch (const std::bad_alloc&) { allocation_budget=-1; failed=true; }
        require(sessions.statistics()==before && sessions.by_sid("SID1") && sessions.by_sid("SID2"), "Failed preparation mutated models");
    }
    require(failed && success, "Allocation campaign missed failure/success");
    const auto plan=prepare(core); std::size_t calls=0;
    const Core::Sink sink=[&](users::SessionId,const Plan&) { ++calls; return true; };
    allocation_budget=0; const auto report=core.dispatch(plan,sink); allocation_budget=-1;
    require(std::get<DispatchReport>(report)==DispatchReport{2,0,0,0} && calls==2, "Dispatch allocated or lost recipients");
    bool thrown=false; calls=0;
    try { (void)core.dispatch(plan,[&](auto,const auto&) -> bool { ++calls; if (calls==2) { throw std::runtime_error("sink"); } return true; }); }
    catch (const std::runtime_error&) { thrown=true; }
    require(thrown && calls==2 && sessions.statistics()==before, "Sink exception retried or mutated registry");
}
}
int main() {
    try { query_validation(); state_and_ownership(); recipients_and_features(); cancellation_and_domains(); fault_campaign();
        std::cout<<"PASS: bounded search criteria, live recipient plans, domains, cancellation and allocation isolation\n"; }
    catch (const std::exception& error) { allocation_budget=-1; std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1; }
}
