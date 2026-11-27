#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/security/roles.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <source_location>
#include <limits>
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
extern "C" int __wrap_RAND_bytes(unsigned char* data, int size) {
    for (int i = 0; i < size; ++i) { data[i] = static_cast<unsigned char>(i); }
    return 1;
}
namespace {
using namespace dc24h;
using namespace dc24h::adc;
using accounts::Role;
using security::Permission;
constexpr std::string_view password = "ADC wire password 42!";
constexpr std::string_view response = "5P4QWFGBOLYMZ6DFRPDKS3FV7Y27NF7XSSZ4QNQ";
void require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("Authentication limit assertion at line " + std::to_string(where.line())); }
}
Message parse(std::string_view text) { return std::get<Message>(parse_line(text)); }
TigerDigest pid(unsigned seed) {
    TigerDigest value{};
    for (std::size_t i = 0; i < value.size(); ++i) { value[i] = static_cast<unsigned char>(i+seed); }
    return value;
}
std::string cid(unsigned seed) { return tiger_base32(*tiger192(pid(seed))); }
struct Source final : AuthenticationSource {
    mutable unsigned preparations{0};
    std::array<Role,7> roles{Role::registered,Role::moderator,Role::administrator,
        Role::registered,Role::moderator,Role::administrator,static_cast<Role>(999)};
    AuthenticationPreparation prepare(std::string_view nick, std::string_view supplied,
        users::Transport, PasswordChallenge& challenge) const override {
        ++preparations;
        for (unsigned i = 1; i <= roles.size(); ++i) {
            if (nick == "Role"+std::to_string(i) || supplied == cid(i)) {
                if (nick != "Role"+std::to_string(i) || supplied != cid(i)) { return DeniedAuthentication{}; }
                return PasswordAuthentication{i,std::get<Message>(challenge.issue(password,SessionHash::tiger192)),roles[i-1]};
            }
        }
        return GuestAuthentication{};
    }
    bool allows_identity(std::string_view, std::string_view, std::optional<std::uint64_t>) const noexcept override { return true; }
};
std::string drain(LoginHub& hub, std::uint64_t id) {
    std::string output;
    while (!hub.output(id).empty()) { const auto wire = hub.output(id); output += wire; (void)hub.consume(id,wire.size()); }
    return output;
}
using Clock = security::AuthenticationLimiter::Clock;
using namespace std::chrono_literals;
security::AuthenticationAddress address(std::string_view text) { return *security::AuthenticationAddress::parse(text); }
std::string identify(LoginHub& hub,std::uint64_t id,unsigned seed,std::string_view peer,
    users::Transport transport,Clock::time_point now) {
    hub.connect(id,peer,transport);
    if (transport==users::Transport::adcs) { hub.transport_ready(id); }
    hub.receive(id,parse("HSUP ADBASE ADTIGR"),now); const auto initial=drain(hub,id);
    const auto sid=initial.substr(initial.find("ISID ")+5,4);
    hub.receive(id,parse("BINF "+sid+" NIRole"+std::to_string(seed)+" ID"+cid(seed)+
        " PD"+tiger_base32(pid(seed))+" I40.0.0.0"),now);
    return sid;
}
void login(LoginHub& hub,std::uint64_t id,unsigned seed,std::string_view peer,
    users::Transport transport,Clock::time_point now) {
    (void)identify(hub,id,seed,peer,transport,now);
    require(drain(hub,id).starts_with("IGPA "));
    hub.receive(id,parse("HPAS "+std::string(response)),now); (void)drain(hub,id); require(hub.ready(id));
}
void validation() {
    security::validate(security::AuthenticationLimits{});
    for (auto field : {0,1,2,3,4,5}) {
        for (auto high : {false,true}) {
            security::AuthenticationLimits limits;
            const auto value=high ? 100001U : 0U;
            if (field==0) { limits.attempts_per_ip=value; }
            if (field==1) { limits.attempts_per_account=value; }
            if (field==2) { limits.attempts_per_second=value; }
            if (field==3) { limits.window=std::chrono::seconds(high ? 3601 : 0); }
            if (field==4) { limits.max_tracked_ips=high ? 65537 : 0; }
            if (field==5) { limits.max_tracked_accounts=high ? 65537 : 0; }
            bool failed=false;try { security::AuthenticationLimiter invalid(limits); } catch (const std::invalid_argument&) { failed=true; }
            require(failed);
        }
    }
    auto maximum=security::AuthenticationLimits{100000,100000,100000,3600s,65536,65536};security::validate(maximum);
    require(address("127.0.0.1")==address("::ffff:127.0.0.1"));
    require(address("2001:db8::1")==address("2001:0DB8:0:0:0:0:0:1"));
    require(address("127.0.0.1")!=address("::127.0.0.1"));
    for (const auto& text : {std::string{},std::string("host"),std::string("127.0.0.1:1511"),
        std::string("::1%eth0"),std::string("127.0.0.1\0junk",14),std::string(1000,'x')}) {
        require(!security::AuthenticationAddress::parse(text));
    }
}
void budgets() {
    const Clock::time_point start{};
    security::AuthenticationLimits limits{2,2,100,10s,10,10};
    security::AuthenticationLimiter limiter(limits);
    require(limiter.admit_ip(address("127.0.0.1"),start));require(limiter.admit_ip(address("::ffff:127.0.0.1"),start));
    require(!limiter.admit_ip(address("127.0.0.1"),start+4999ms));
    require(limiter.admit_ip(address("127.0.0.1"),start+5s));
    require(!limiter.admit_ip(address("127.0.0.1"),start)); // Backwards time cannot mint credit.
    require(limiter.admit_ip(address("127.0.0.2"),start+5s));
    security::AuthenticationLimiter accounts(limits);
    require(accounts.admit_account(42,start) && accounts.admit_account(42,start));
    require(!accounts.admit_account(42,start+4999ms) && accounts.admit_account(43,start+4999ms));
    require(accounts.admit_account(42,start+5s) && !accounts.admit_account(42,start));
    require(!accounts.admit_account(0,start) && !accounts.admit_account(std::numeric_limits<std::uint64_t>::max(),start));
    limits.attempts_per_second=2;security::AuthenticationLimiter global(limits);
    require(global.admit_ip(address("127.0.0.1"),start) && global.admit_ip(address("127.0.0.2"),start));
    require(!global.admit_ip(address("127.0.0.3"),start+499ms) && global.tracked_ips()==2);
    require(global.admit_ip(address("127.0.0.3"),start+500ms));
    // Full signed clock horizons and maximum configured products cannot overflow.
    security::AuthenticationLimiter extreme({100000,100000,100000,3600s,2,2});
    require(extreme.admit_ip(address("::1"),Clock::time_point::min()));
    require(extreme.admit_account(42,Clock::time_point::min()));
    require(extreme.admit_ip(address("::1"),Clock::time_point::max()));
    require(extreme.admit_account(42,Clock::time_point::max()));
    require(extreme.admit_account(42,Clock::time_point::min()));
}
void capacity_and_faults() {
    const Clock::time_point start{};
    security::AuthenticationLimiter limiter({1,1,100,10s,2,2});
    require(limiter.admit_ip(address("127.0.0.1"),start) && limiter.admit_ip(address("127.0.0.2"),start));
    require(limiter.admit_account(1,start) && limiter.admit_account(2,start));
    require(!limiter.admit_ip(address("127.0.0.3"),start+9s) && !limiter.admit_account(3,start+9s));
    require(limiter.tracked_ips()==2 && limiter.tracked_accounts()==2);
    require(limiter.admit_ip(address("127.0.0.3"),start+10s) && limiter.admit_account(3,start+10s));
    require(limiter.tracked_ips()==1 && limiter.tracked_accounts()==1);
    security::AuthenticationLimiter allocation({2,1,1,10s,2,2});
    fail_allocation=true;bool failed=false;
    try { (void)allocation.admit_ip(address("::1"),start); } catch (const std::bad_alloc&) { failed=true; }
    fail_allocation=false;require(failed && allocation.tracked_ips()==0);
    require(allocation.admit_ip(address("::1"),start)); // Failed allocation did not debit global credit.
    fail_allocation=true;require(!allocation.admit_ip(address("::2"),start));fail_allocation=false;
    fail_allocation=true;failed=false;
    try { (void)allocation.admit_account(1,start); } catch (const std::bad_alloc&) { failed=true; }
    fail_allocation=false;require(failed && allocation.tracked_accounts()==0 && allocation.admit_account(1,start));
    fail_allocation=true;require(!allocation.admit_account(1,start));fail_allocation=false;
    fail_allocation=true;require(allocation.admit_ip(address("::1"),start+1s));fail_allocation=false;
}
void hub_limits() {
    const auto start=Clock::now();
    LoginLimits limits;auto source=std::make_shared<Source>();limits.authentication=source;
    limits.authentication_limits={2,100,100,10s,10,10};LoginHub hub(limits);
    for (auto id : {1U,2U}) { login(hub,id,3,"127.0.0.1",users::Transport::adcs,start);hub.disconnect(id,false); }
    (void)identify(hub,3,3,"::ffff:127.0.0.1",users::Transport::adc,start);
    const auto refused=drain(hub,3);
    require(refused.starts_with("ISTA 223 ") && refused.find("IGPA ")==std::string::npos && source->preparations==2);
    require(hub.closing(3) && hub.session_model(3).terminal_cause()==users::CloseCause::authentication_limit);
    require(!hub.authenticated_session(3));hub.disconnect(3,false);
    login(hub,4,3,"127.0.0.2",users::Transport::adc,start);hub.disconnect(4,false);
    // Shared account quota across source IPs, reconnects, transports and roles.
    limits.authentication_limits={100,2,100,10s,10,10};LoginHub accounts(limits);
    login(accounts,1,3,"127.0.0.1",users::Transport::adcs,start);accounts.disconnect(1,false);
    login(accounts,2,3,"127.0.0.2",users::Transport::adc,start);accounts.disconnect(2,false);
    (void)identify(accounts,3,3,"127.0.0.3",users::Transport::adcs,start);
    require(drain(accounts,3).starts_with("ISTA 223 ") && !accounts.authenticated_session(3));accounts.disconnect(3,false);
    login(accounts,4,1,"127.0.0.3",users::Transport::adcs,start);accounts.disconnect(4,false);
    (void)identify(accounts,5,3,"127.0.0.4",users::Transport::adcs,start+4s);
    require(drain(accounts,5).starts_with("ISTA 223 "));accounts.disconnect(5,false);
    login(accounts,6,3,"127.0.0.4",users::Transport::adcs,start+5s);accounts.disconnect(6,false);
}
}
int main() {
    try { validation();budgets();capacity_and_faults();hub_limits();std::cout << "Authentication limits, shared budgets, canonical IPs and fault checks passed\n"; }
    catch (const std::exception& error) { fail_allocation=false;std::cerr << error.what() << '\n';return 1; }
}
