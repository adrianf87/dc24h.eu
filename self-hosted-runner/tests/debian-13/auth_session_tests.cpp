#include "dc24h/users/session_manager.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>

namespace { bool fail_allocation = false; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_allocation) { throw std::bad_alloc(); }
    if (auto* value = std::malloc(size ? size : 1)) { return value; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

namespace {
using namespace dc24h::users;
using namespace std::chrono_literals;
void require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("Auth session assertion at line " + std::to_string(where.line())); }
}
SessionId id(std::uint64_t value) { return *SessionId::from_value(value); }
dc24h::accounts::AccountId account(std::uint64_t value = 42) { return *dc24h::accounts::AccountId::from_value(value); }
void identify(SessionManager& manager, std::uint64_t value, Transport transport = Transport::adcs) {
    require(std::holds_alternative<const Session*>(manager.add(id(value),transport,{})));
    require(!manager.start_negotiation(id(value),TimePoint{}+1ms));
    const auto name = std::to_string(value);
    require(!manager.identify(id(value),*User::create(*Identity::create("S"+name,"C"+name,"User"+name)),TimePoint{}+2ms));
}
void lifecycle() {
    SessionManager manager(4);
    require(!manager.authenticated(id(1)));
    require(manager.mark_authenticated_ready(id(1),account(),{}).has_value());
    require(std::holds_alternative<const Session*>(manager.add(id(1),Transport::adcs,{})));
    require(manager.mark_authenticated_ready(id(1),account(),TimePoint{}+1ms).has_value());
    require(!manager.start_negotiation(id(1),TimePoint{}+1ms));
    require(!manager.identify(id(1),*User::create(*Identity::create("AAAB","CID","Registered")),TimePoint{}+2ms));
    const auto before = manager.snapshot(id(1));
    require(manager.mark_authenticated_ready(id(1),account(),TimePoint{}+1ms).has_value());
    require(manager.snapshot(id(1)) == before && !manager.authenticated(id(1)));
    fail_allocation = true;
    const auto ready = manager.mark_authenticated_ready(id(1),account(),TimePoint{}+3ms);
    const auto auth = manager.authenticated(id(1));
    fail_allocation = false;
    require(!ready && auth && auth->session_id == id(1) && auth->account_id == account());
    require(auth->transport == Transport::adcs && auth->established_at == TimePoint{}+3ms);
    require(manager.snapshot(id(1))->authentication == auth && manager.counts().ready == 1);
    require(manager.mark_authenticated_ready(id(1),account(43),TimePoint{}+4ms).has_value());
    require(manager.mark_ready(id(1),TimePoint{}+4ms).has_value() && manager.authenticated(id(1)) == auth);
    const auto saved = manager.snapshot(id(1));
    require(manager.rename(id(1),"Impersonation",TimePoint{}+4ms).has_value());
    require(manager.snapshot(id(1)) == saved && manager.by_nickname("Registered") && !manager.by_nickname("Impersonation"));
    require(manager.begin_close(id(1),CloseCause::shutdown,TimePoint{}+2ms).has_value());
    require(manager.authenticated(id(1)) == auth);
    fail_allocation = true;
    const auto closed = manager.begin_close(id(1),CloseCause::shutdown,TimePoint{}+4ms);
    fail_allocation = false;
    require(!closed && !manager.authenticated(id(1)) && !manager.snapshot(id(1))->authentication);
    require(saved->authentication == auth); // Owning historical data cannot restore readiness.
    require(!manager.finish_close(id(1),CloseCause::peer_closed,TimePoint{}+5ms) && !manager.remove(id(1)));
    require(!manager.authenticated(id(1)) && std::get<RegistryError>(manager.add(id(1),Transport::adc,{})) == RegistryError::stale_id);
    identify(manager,2,Transport::adc);
    require(!manager.mark_ready(id(2),TimePoint{}+3ms));
    require(!manager.authenticated(id(2)) && manager.mark_authenticated_ready(id(2),account(),TimePoint{}+4ms).has_value());
    require(manager.authenticated_page(account()).empty());
}
void pages_and_cleanup() {
    SessionManager manager(5);
    for (auto value : {1U,3U,7U,9U}) {
        identify(manager,value,value == 7 ? Transport::adc : Transport::adcs);
        require(!manager.mark_authenticated_ready(id(value),account(value == 3 ? 43 : 42),TimePoint{}+3ms));
    }
    const auto page = manager.authenticated_page(account(),0,2);
    require(page.size() == 2 && page[0].session_id == id(1) && page[1].session_id == id(7));
    require(page[1].transport == Transport::adc);
    require(manager.authenticated_page(account(),7,1)[0].session_id == id(9));
    require(manager.authenticated_page(account(),std::numeric_limits<std::uint64_t>::max()).empty());
    require(manager.authenticated_page(account(43)).size() == 1 && manager.authenticated_page(account(44)).empty());
    for (auto limit : {std::size_t{0},SessionManager::max_page_size+1}) {
        bool rejected = false;
        try { (void)manager.authenticated_page(account(),0,limit); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected);
    }
    fail_allocation = true;
    bool rejected = false;
    try { (void)manager.authenticated_page(account()); } catch (const std::bad_alloc&) { rejected = true; }
    fail_allocation = false;
    require(rejected && manager.authenticated_page(account()).size() == 3);
    require(!manager.force_close(id(1),CloseCause::output_limit,TimePoint{}+4ms));
    require(!manager.finish_close(id(7),CloseCause::peer_closed,TimePoint{}+4ms));
    require(!manager.begin_close(id(9),CloseCause::authentication_revoked,TimePoint{}+4ms));
    require(manager.authenticated_page(account()).empty() && manager.authenticated(id(3)));
    require(page[0].account_id == account() && page[1].session_id == id(7));
}
}
int main() {
    try { lifecycle(); pages_and_cleanup(); std::cout << "Auth session lifecycle, isolation, paging and fault checks passed\n"; }
    catch (const std::exception& error) { fail_allocation = false; std::cerr << error.what() << '\n'; return 1; }
}
