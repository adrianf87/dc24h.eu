#include "dc24h/search/core.hpp"
#include "dc24h/adc/sch.hpp"

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
using namespace std::chrono_literals;
void require(bool ok, const char* why) { if (!ok) { throw std::runtime_error(why); } }
users::SessionId id(std::uint64_t n) { return *users::SessionId::from_value(n); }
users::TimePoint at(std::int64_t seconds) { return users::TimePoint{} + std::chrono::seconds{seconds}; }
void add(users::SessionManager& sessions, std::uint64_t n, int phase = 3, std::string source = {}) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n),users::Transport::adc,{})), "Session add failed");
    if (phase == 0) { return; }
    require(!sessions.start_negotiation(id(n),{}), "Negotiation failed");
    if (phase == 1) { return; }
    auto identity = *users::Identity::create(source.empty() ? "SID"+std::to_string(n) : source,"CID"+std::to_string(n),"Nick"+std::to_string(n));
    require(!sessions.identify(id(n),*users::User::create(identity),{}), "Identify failed");
    if (phase == 3) { require(!sessions.mark_ready(id(n),{}), "Readiness failed"); }
}
TokenHandle activate(Tokens& tokens, std::uint64_t owner, std::string_view value, users::TimePoint now = at(0)) {
    return std::get<TokenHandle>(tokens.activate(id(owner),value,now));
}
QueryData query(std::string token = "opaque token") {
    QueryData data; data.include = {"Ubuntu"}; data.token = std::move(token); return data;
}
ActivePlan start(Core& core, QueryData data = query(), users::TimePoint now = at(0)) {
    return std::get<ActivePlan>(core.start(id(1),"SID1",data,now));
}
void validation_and_scopes() {
    users::SessionManager sessions(16); add(sessions,1); add(sessions,2);
    Tokens tokens(sessions,{128,64,60s});
    for (const auto& value : std::vector<std::string>{"Case", "case", "Żółw token", "a\\b", "   ", std::string(max_token_bytes,'x')}) {
        const auto handle = activate(tokens,1,value);
        require(handle.token() == value && tokens.find(id(1),value,at(0)) == handle.search(), "Opaque token spelling changed");
    }
    require(!tokens.find(id(1),"CASE",at(0)), "Lookup folded token case");
    for (const auto& value : std::vector<std::string>{"", "bad\n", "bad\t", "e\xcc\x81", std::string("bad\0x",5),
        std::string(1,char(0xff)), "bad\xe2\x80\xae", std::string(max_token_bytes+1,'x')}) {
        require(std::get<TokenError>(tokens.activate(id(1),value,at(0))) == TokenError::invalid_token, "Invalid token accepted");
    }
    auto huge = std::string(1024*1024,'x');
    allocation_budget = 0; const auto bounded = tokens.activate(id(1),huge,at(0)); allocation_budget = -1;
    require(std::get<TokenError>(bounded) == TokenError::invalid_token, "Token limit allocated before refusal");
    const auto first = activate(tokens,1,"shared"); const auto second = activate(tokens,2,"shared");
    require(first.search().id != second.search().id && first.search().owner != second.search().owner, "Owners shared one search identity");
    require(std::get<TokenError>(tokens.activate(id(1),"shared",at(59))) == TokenError::duplicate_token, "Duplicate active TO accepted");
    require(tokens.find(id(1),"shared",at(59))->expires == at(60), "Duplicate refreshed the lifetime");
    require(!tokens.find(id(1),"shared",at(60)) && !tokens.active(first,at(0)), "Expiry boundary or backwards clock resurrected a token");
    const auto reused = activate(tokens,1,"shared",at(0));
    require(reused.search().expires == at(120) && reused.search().id != first.search().id, "Clock clamp/ID generation failed");
    require(!tokens.cancel(first) && tokens.active(reused,at(60)), "Stale handle canceled a replacement");
    Tokens foreign(sessions); const auto other = activate(foreign,1,"shared");
    require(!tokens.cancel(other) && !foreign.cancel(reused), "Foreign token domain accepted a handle");
    for (int phase = 0; phase < 3; ++phase) {
        add(sessions,static_cast<std::uint64_t>(phase+3),phase);
        require(std::get<TokenError>(tokens.activate(id(static_cast<std::uint64_t>(phase+3)),"x",at(60))) == TokenError::owner_not_ready,
            "Unready owner activated a search");
    }
    require(std::get<TokenError>(tokens.activate(id(99),"x",at(60))) == TokenError::missing_owner, "Missing owner accepted");
}
void bounds_cleanup_and_exhaustion() {
    users::SessionManager sessions(8); for (std::uint64_t n=1;n<=3;++n) { add(sessions,n); }
    for (const auto limits : std::vector<TokenLimits>{{0,1,1s},{65536,1,1s},{1,0,1s},{64,65,1s},{1,2,1s},{1,1,0s},{1,1,3601s}}) {
        bool refused = false; try { Tokens bad(sessions,limits); } catch (const std::invalid_argument&) { refused = true; }
        require(refused,"Invalid registry limits accepted");
    }
    Tokens tokens(sessions,{3,2,60s}); const auto a = activate(tokens,1,"a"); const auto b = activate(tokens,1,"b");
    require(std::get<TokenError>(tokens.activate(id(1),"c",at(0))) == TokenError::owner_limit, "Owner cap bypassed");
    const auto c = activate(tokens,2,"c");
    require(std::get<TokenError>(tokens.activate(id(3),"d",at(0))) == TokenError::total_limit, "Global cap bypassed");
    require(!sessions.begin_close(id(2),users::CloseCause::peer_closed,{}), "Close failed");
    const auto d = activate(tokens,3,"d");
    require(tokens.size() == 3 && !tokens.active(c,at(0)), "Capacity did not reclaim a closed owner");
    auto copy = a; auto moved = std::move(copy);
    allocation_budget = 0;
    const auto found = tokens.find(id(1),"a",at(0)); const bool canceled = tokens.cancel(moved);
    const auto forgotten = tokens.forget(id(1)); const auto expired = tokens.prune(at(60));
    allocation_budget = -1;
    require(found == a.search() && canceled && forgotten == 1 && expired == 1 && tokens.size() == 0, "Cleanup allocated or lost an index entry");
    require(!tokens.cancel(b) && !tokens.cancel(d), "Cleanup retained stale handles");
    require(!sessions.finish_close(id(2),users::CloseCause::peer_closed,{}) && !sessions.remove(id(2)), "Session cleanup failed");
    add(sessions,4,3,"SID2"); const auto replacement = activate(tokens,4,"c",at(60));
    require(!tokens.find(id(2),"c",at(60)) && !tokens.cancel(c) && tokens.active(replacement,at(60)), "Reused SID replaced a lifetime owner");
    Tokens last(sessions,{2,2,1s},std::numeric_limits<std::uint64_t>::max());
    const auto final = activate(last,1,"last");
    require(final.search().id == std::numeric_limits<std::uint64_t>::max(), "Last generation was skipped");
    require(last.cancel(final) && std::get<TokenError>(last.activate(id(1),"next",at(0))) == TokenError::id_exhausted, "ID stream wrapped");
    Tokens extreme(sessions,{2,2,1s}); const auto near = users::TimePoint::max()-users::Clock::duration{1};
    const auto saturated = activate(extreme,1,"near",near);
    require(saturated.search().expires == users::TimePoint::max() && !extreme.find(id(1),"near",users::TimePoint::max()), "Deadline overflow/boundary failed");
    require(std::get<TokenError>(extreme.activate(id(1),"max",users::TimePoint::max())) == TokenError::time_exhausted, "Zero-length saturated lifetime accepted");
    Tokens minimum(sessions,{1,1,1s});
    require(activate(minimum,1,"min",users::TimePoint::min()).search().expires == users::TimePoint::min()+1s, "Negative clock origin overflowed");
}
void lifetime_and_domains() {
    users::SessionManager sessions(2); add(sessions,1);
    alignas(Tokens) unsigned char storage[sizeof(Tokens)];
    auto* first = ::new(static_cast<void*>(storage)) Tokens(sessions); const auto old = activate(*first,1,"same"); first->~Tokens();
    auto* next = ::new(static_cast<void*>(storage)) Tokens(sessions); const auto current = activate(*next,1,"same");
    require(old.search() == current.search() && !next->cancel(old) && next->active(current,at(0)), "Reused registry address accepted an old domain");
    next->~Tokens();
    Tokens tokens(sessions); const auto handle = activate(tokens,1,"close");
    require(!sessions.begin_close(id(1),users::CloseCause::peer_closed,{}), "Closing failed");
    require(!tokens.find(id(1),"close",at(0)) && tokens.size() == 0 && !tokens.active(handle,at(0)), "Closing owner remained searchable");
}
void core_lifecycle() {
    users::SessionManager sessions(4); for (std::uint64_t n=1;n<=3;++n) { add(sessions,n); }
    Core core(sessions); auto data = query();
    require(std::get<Error>(core.start(id(1),"wrong",{},at(0))) == Error::wrong_source, "Token checks preceded source ownership");
    auto absent = data; absent.token.reset();
    require(std::get<TokenError>(core.start(id(1),"SID1",absent,at(0))) == TokenError::missing_token, "Active search fabricated TO");
    auto empty = data; empty.include.clear();
    require(std::get<QueryError>(core.start(id(1),"SID1",empty,at(0))) == QueryError::no_criteria && !core.find_search(id(1),*data.token,at(0)),
        "Rejected criteria reserved a token");
    auto active = start(core); auto copy = active;
    data.include[0] = "mutated"; data.token = "changed";
    require(active.plan().query().data() == query() && core.find_search(id(1),"opaque token",at(0)) == active.handle().search(), "Active search borrowed input");
    require(std::get<TokenError>(core.start(id(1),"SID1",query(),at(0))) == TokenError::duplicate_token, "Core refreshed an active TO");
    std::size_t calls = 0; const Core::Sink sink = [&](auto,const Plan&) { ++calls; return true; };
    allocation_budget = 0; const auto report = core.dispatch(active,sink,at(0)); allocation_budget = -1;
    require(std::get<DispatchReport>(report) == DispatchReport{3,0,0,0} && calls == 3, "Active dispatch allocated or missed recipients");
    require(core.find_search(id(1),"opaque token",at(0)).has_value(), "Queue dispatch prematurely completed the search");
    Core foreign(sessions);
    require(std::get<Error>(foreign.dispatch(active,sink,at(0))) == Error::foreign_plan && !foreign.complete(active), "Foreign Core accepted a tracked search");
    require(core.complete(copy) && !core.complete(active), "Completion was not shared/idempotent");
    auto replacement = start(core);
    require(!core.complete(active) && replacement.handle().search().id != active.handle().search().id, "Old completion removed token reuse");
    require(std::get<TokenError>(core.dispatch(active,sink,at(0))) == TokenError::inactive_search, "Canceled search dispatched");
    const Core::Sink cancel = [&](auto,const Plan&) { require(core.complete(replacement),"Mid-dispatch completion failed"); return true; };
    require(std::get<DispatchReport>(core.dispatch(replacement,cancel,at(0))) == DispatchReport{1,0,0,2}, "Completion did not cancel remaining recipients");
    const auto wire = std::get<adc::SchRequest>(adc::parse_sch("BSCH AAAB TR"+std::string(39,'A')+" TOŻółw\\stoken"));
    auto hash = start(core,wire.criteria());
    require(hash.plan().query().data().tth == TthRoot{} && hash.handle().token() == "Żółw token", "SCH/TTH token projection changed");
    require(core.prune_searches(at(60)) == 1 && std::get<TokenError>(core.dispatch(hash,sink,at(60))) == TokenError::inactive_search, "Expired search dispatched");
}
void allocation_campaigns() {
    users::SessionManager sessions(2); add(sessions,1); const auto token = std::string(128,'x');
    bool failed = false, success = false;
    for (long budget=0;budget<100 && !success;++budget) {
        Tokens tokens(sessions); const auto keep = activate(tokens,1,"keep"); bool threw = false;
        allocation_budget = budget;
        try { const auto result = tokens.activate(id(1),token,at(0)); allocation_budget = -1;
            require(std::get<TokenHandle>(result).search().id == 2,"Activation skipped an ID"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1; threw = failed = true; }
        require(tokens.active(keep,at(0)) && tokens.size() == (threw ? 1U : 2U), "Allocation failure left partial registry state");
        if (threw) { require(!tokens.find(id(1),token,at(0)) && activate(tokens,1,token).search().id == 2,"Rollback leaked token/ID"); }
        require(tokens.prune(at(60)) == 2 && tokens.size() == 0,"Deadline index rollback failed");
    }
    require(failed && success,"Registry campaign missed allocation failures/success");
    failed = success = false; const auto before = sessions.statistics();
    for (long budget=0;budget<150 && !success;++budget) {
        Core core(sessions); auto data = query(token); bool threw = false;
        allocation_budget = budget;
        try { const auto result = core.start(id(1),"SID1",data,at(0)); allocation_budget = -1;
            require(std::holds_alternative<ActivePlan>(result),"Start fault returned a validation error"); success = true;
        } catch (const std::bad_alloc&) { allocation_budget = -1; threw = failed = true; }
        require(sessions.statistics() == before && (!threw || !core.find_search(id(1),token,at(0))), "Failed Core start mutated models or reserved a token");
        if (threw) { require(start(core,data).handle().search().id == 1,"Failed start consumed an ID"); }
    }
    require(failed && success,"Core campaign missed allocation failures/success");
}
} // namespace
int main() {
    try { validation_and_scopes(); bounds_cleanup_and_exhaustion(); lifetime_and_domains(); core_lifecycle(); allocation_campaigns();
        std::cout << "PASS: session-bound search tokens, deadlines, generations, Core lifecycle and allocation rollback\n";
    } catch (const std::exception& error) { allocation_budget = -1; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
