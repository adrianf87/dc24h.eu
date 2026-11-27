#include "dc24h/search/features.hpp"
#include "dc24h/adc/search_routing.hpp"
#include "dc24h/adc/sid.hpp"

#include <algorithm>
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
using namespace dc24h::search;
void require(bool value, const char* why) { if (!value) { throw std::runtime_error(why); } }
FeatureFilter compile(const Selection& selection) { return std::get<FeatureFilter>(FeatureFilter::create(selection)); }
void names_and_validation() {
    for (const auto name : {"BASE", "TIGR", "ADC0", "TCP4", "UDP6", "ZZZ9"}) {
        require(FeatureCode::parse(name).has_value(), "Valid or unknown feature refused");
    }
    for (const auto name : {"", "TCP", "TCP44", "tcp4", "Tcp4", "T2P4", "2CP4", "TC_4", "TC+4", "ŻCP4"}) {
        require(!FeatureCode::parse(name), "Malformed feature accepted");
    }
    require(!FeatureCode::parse(std::string("TCP\0",4)), "NUL feature accepted");
    const std::vector<std::pair<Selection, FeatureError>> invalid{
        {{{"TCP4", "TCP4"}, {}}, FeatureError::duplicate},
        {{{}, {"UDP4", "UDP4"}}, FeatureError::duplicate},
        {{{"TCP4"}, {"TCP4"}}, FeatureError::conflict},
        {{{std::string(1024*1024,'A')}, {}}, FeatureError::invalid_name},
        {{{}, {"TCP"}}, FeatureError::invalid_name},
        {{std::vector<std::string>(33,"TCP4"), {}}, FeatureError::too_many}};
    for (const auto& [selection, expected] : invalid) {
        allocation_budget = 0;
        const auto result = FeatureFilter::create(selection);
        allocation_budget = -1;
        require(std::get<FeatureError>(result) == expected, "Validation copied hostile strings or wrong error");
    }
    Selection maximum;
    for (int n=0; n<32; ++n) {
        std::string code="AAA0"; code[1]=static_cast<char>('A'+n/26); code[2]=static_cast<char>('A'+n%26);
        maximum.required_features.push_back(code);
    }
    const auto all = compile(maximum);
    allocation_budget = 0; const bool matched = all.matches(maximum.required_features); allocation_budget = -1;
    require(matched, "Required bit 31 lost or matching allocated");
    auto missing = maximum.required_features; missing.pop_back();
    require(!all.matches(missing), "Partial required set accepted");
    maximum.excluded_features = {"ZZZ9"};
    require(std::get<FeatureError>(FeatureFilter::create(maximum)) == FeatureError::too_many, "Combined selector limit bypassed");
    maximum.excluded_features.clear(); maximum.excluded_features.swap(maximum.required_features);
    require(!compile(maximum).matches(missing) && compile(maximum).matches({}), "All-excluded mask differs");
}
void matching_and_ownership() {
    const std::vector<std::string> labels{"ADC0","TCP4","UDP4","ZZZ9"};
    // Compare all disjoint required/excluded subsets against the literal rule,
    // across every public SU subset. This also exercises negative-only filters.
    for (unsigned required=0; required<16; ++required) {
        for (unsigned excluded=0; excluded<16; ++excluded) {
            if ((required & excluded) != 0) { continue; }
            Selection selection;
            for (unsigned i=0; i<4; ++i) {
                if (required & (1U<<i)) { selection.required_features.push_back(labels[i]); }
                if (excluded & (1U<<i)) { selection.excluded_features.push_back(labels[i]); }
            }
            const auto filter = compile(selection);
            for (unsigned mask=0; mask<16; ++mask) {
                std::vector<std::string> advertised;
                for (unsigned i=0; i<4; ++i) { if (mask & (1U<<i)) { advertised.push_back(labels[i]); } }
                allocation_budget=0; const bool result=filter.matches(advertised); allocation_budget=-1;
                require(result == ((mask & required) == required && (mask & excluded) == 0), "Feature boolean rule differs");
            }
        }
    }
    Selection selection{{"ZZZ9", "ADC0"}, {"UDP4"}};
    auto filter=compile(selection); const auto original=selection;
    selection.required_features.clear(); selection.excluded_features.clear();
    require(filter.selection()==original, "Filter borrows caller selectors");
    auto copy=filter; auto moved=std::move(copy);
    const std::vector<std::string> public_features{"ADC0","ZZZ9","ADC0","ZZZ8","tcp4","T2P4"};
    require(filter.matches(public_features) && moved.matches(public_features), "Unknown/exact/copy matching differs");
    require(!filter.matches(std::vector<std::string>{"ADC0","ZZZ8"}), "Feature revision confused");
    auto denied=public_features; denied.push_back("UDP4");
    require(!moved.matches(denied), "Excluded feature lost after move");
    bool failed=false, succeeded=false;
    for (long budget=0; budget<16 && !succeeded; ++budget) {
        allocation_budget=budget;
        try { auto result=FeatureFilter::create(original); allocation_budget=-1; succeeded=std::holds_alternative<FeatureFilter>(result); }
        catch (const std::bad_alloc&) { allocation_budget=-1; failed=true; }
        require(original==filter.selection(), "Failed compile mutated caller or existing filter");
    }
    require(failed && succeeded, "Owning compile allocation campaign incomplete");
}
users::SessionId id(std::uint64_t n) { return *users::SessionId::from_value(n); }
void add(users::SessionManager& sessions, std::uint64_t n, std::vector<std::string> features) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(n),users::Transport::adc,{})), "Add failed");
    require(!sessions.start_negotiation(id(n),{}), "Negotiation failed");
    users::ProfileData data; data.features=std::move(features);
    require(!sessions.identify(id(n),*users::User::create(*users::Identity::create(
        adc::Sid::from_value(static_cast<std::uint32_t>(n))->str(),"CID"+std::to_string(n),"Nick"+std::to_string(n)),
        *users::Profile::create(data)),{}), "Identify failed");
    require(!sessions.mark_ready(id(n),{}), "Ready failed");
}
void update(users::SessionManager& sessions, std::uint64_t n, std::vector<std::string> features) {
    auto data=sessions.find(id(n))->user()->profile().data(); data.features=std::move(features);
    require(!sessions.update_user(id(n),*users::User::create(*sessions.find(id(n))->identity(),*users::Profile::create(data)),{}), "Update failed");
}
void live_rechecks_and_multipage() {
    users::SessionManager sessions(300);
    for (std::uint64_t n=1; n<=270; ++n) { add(sessions,n,n%2==0?std::vector<std::string>{"ADC0","ZZZ9"}:std::vector<std::string>{"TCP4"}); }
    Core core(sessions); adc::SearchRouter router(core);
    auto message=std::get<adc::Message>(adc::parse_line("FSCH AAAB +ZZZ9+ADC0-TCP4 ANFeaturePrivate TOFeaturePrivateToken XXopaque"));
    auto prepared=std::get<adc::RoutedSearch>(router.prepare(id(1),message,{}));
    require(prepared.plan().recipients().size()==135 && prepared.plan().selection()==Selection{{"ZZZ9","ADC0"},{"TCP4"}}, "FSCH plan selection differs");
    update(sessions,2,{"ADC0","ZZZ9","TCP4"}); // A newly excluded feature cancels delivery.
    update(sessions,4,{"ADC0"}); // Losing either required feature cancels delivery.
    update(sessions,3,{"ADC0","ZZZ9"}); // A capability gain must not expand a retained snapshot.
    add(sessions,271,{"ADC0","ZZZ9"}); // Nor may a later join enter it.
    std::size_t count=0;
    const adc::SearchRouter::Sink sink=[&](auto recipient,const auto& frame) {
        require(recipient.value()>=6 && recipient.value()%2==0 && frame.get()==prepared.frame().get(), "Wrong recipient or copied frame");
        ++count; return true;
    };
    allocation_budget=0; const auto dispatched=router.dispatch(prepared,sink,{}); allocation_budget=-1;
    require(std::get<DispatchReport>(dispatched)==DispatchReport{133,2,0,0} && count==133, "Dispatch failed to recheck compiled selectors without allocation");
    require(router.cancel(prepared), "Feature token cleanup failed");
    message.features.push_back({false,"ZZZ9"});
    require(std::get<adc::SearchRoutingError>(router.prepare(id(1),message,{}))==adc::SearchRoutingError::invalid_message &&
        !core.find_search(id(1),"FeaturePrivateToken",{}), "Conflicting FSCH reserved a token");
}
}
int main() {
    try { names_and_validation(); matching_and_ownership(); live_rechecks_and_multipage(); }
    catch (const std::exception& error) { allocation_budget=-1; std::cerr<<error.what()<<'\n'; return 1; }
}
