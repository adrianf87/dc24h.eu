#include "dc24h/adc/authentication.hpp"
#include "dc24h/adc/login.hpp"
#include <gcrypt.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>
#include <source_location>

namespace {
bool random_failure = false, hash_failure = false, deterministic = true, scrubbed = true;
int fail_after = -1; unsigned comparisons = 0;
}
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (fail_after == 0) { fail_after = -1; throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* p = std::malloc(size ? size : 1)) { return p; } throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p,std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
extern "C" int __real_RAND_bytes(unsigned char*,int);
extern "C" int __wrap_RAND_bytes(unsigned char* data,int size) {
    if (random_failure) { return 0; }
    if (!deterministic) { return __real_RAND_bytes(data,size); }
    for (int i = 0; i < size; ++i) { data[i] = static_cast<unsigned char>(i); } return 1;
}
extern "C" gcry_error_t __real_gcry_md_open(gcry_md_hd_t*,int,unsigned int);
extern "C" gcry_error_t __wrap_gcry_md_open(gcry_md_hd_t* ctx,int algorithm,unsigned int flags) {
    if (hash_failure) { *ctx = nullptr; return 1; } return __real_gcry_md_open(ctx,algorithm,flags);
}
extern "C" int __real_CRYPTO_memcmp(const void*,const void*,std::size_t);
extern "C" int __wrap_CRYPTO_memcmp(const void* a,const void* b,std::size_t size) {
    ++comparisons; return __real_CRYPTO_memcmp(a,b,size);
}
extern "C" void __real_OPENSSL_cleanse(void*,std::size_t);
extern "C" void __wrap_OPENSSL_cleanse(void* p,std::size_t size) {
    __real_OPENSSL_cleanse(p,size); const auto* bytes = static_cast<const unsigned char*>(p);
    scrubbed = scrubbed && std::all_of(bytes,bytes+size,[](auto b) { return b == 0; });
}
namespace {
using namespace dc24h::adc;
constexpr std::string_view password = "ADC wire password 42!";
constexpr std::string_view response = "5P4QWFGBOLYMZ6DFRPDKS3FV7Y27NF7XSSZ4QNQ";
void require(bool value,std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("ADC authentication assertion at line "+std::to_string(where.line())); }
}
Message parse(std::string_view text) {
    auto parsed = parse_line(text); require(std::holds_alternative<Message>(parsed)); return std::get<Message>(std::move(parsed));
}
void codec() {
    const auto good = parse("HPAS "+std::string(response));
    PasswordChallenge challenge;
    const auto issued = challenge.issue(password,SessionHash::tiger192);
    require(std::holds_alternative<Message>(issued));
    require(std::get<Message>(issued).parameters[0] == "AAAQEAYEAUDAOCAJBIFQYDIOB4IBCEQTCQKRMFY");
    require(challenge.sent() && !challenge.sent());
    const auto before = comparisons; require(!challenge.verify(good)); require(comparisons == before+1);
    require(challenge.verify(good) == AuthenticationError::sequence);
    require(std::get<AuthenticationError>(challenge.issue(password,SessionHash::tiger192)) == AuthenticationError::sequence);
    for (const auto& wire : {std::string("HPAS AAA"),"HPAS "+std::string(39,'a'),
                            "HPAS "+std::string(38,'A')+'B',"HPAS "+std::string(response)+" XXextra"}) {
        PasswordChallenge candidate; require(std::holds_alternative<Message>(candidate.issue(password,SessionHash::tiger192)));
        require(candidate.sent()); require(candidate.verify(parse(wire)) == AuthenticationError::invalid_response);
        require(candidate.verify(good) == AuthenticationError::sequence);
    }
    { PasswordChallenge candidate; (void)candidate.issue(password,SessionHash::tiger192);
      require(candidate.verify(good) == AuthenticationError::sequence); require(!candidate.sent()); }
    { PasswordChallenge candidate; (void)candidate.issue(password,SessionHash::tiger192); require(candidate.sent());
      require(candidate.verify(parse("HPAS "+std::string(39,'A'))) == AuthenticationError::mismatch); }
    for (const auto& invalid : {std::string{},std::string(1025,'x'),std::string("bad\npassword"),std::string("bad\xffpassword")}) {
        PasswordChallenge candidate; require(std::get<AuthenticationError>(candidate.issue(invalid,SessionHash::tiger192)) == AuthenticationError::invalid_password);
    }
    for (int error = 0; error < 2; ++error) {
        random_failure = error == 0; hash_failure = error == 1;
        PasswordChallenge candidate; require(std::get<AuthenticationError>(candidate.issue(password,SessionHash::tiger192)) == AuthenticationError::crypto);
        require(!candidate.sent()); random_failure = hash_failure = false;
    }
    bool success = false;
    for (int index = 0; index < 32 && !success; ++index) {
        PasswordChallenge candidate; fail_after = index;
        try { auto value = candidate.issue(password,SessionHash::tiger192); fail_after = -1; require(std::holds_alternative<Message>(value)); success = true; }
        catch (const std::bad_alloc&) { fail_after = -1; require(!candidate.pending()); }
    }
    require(success);
    deterministic = false;
    PasswordChallenge a,b; const auto first = std::get<Message>(a.issue(password,SessionHash::tiger192));
    const auto second = std::get<Message>(b.issue(password,SessionHash::tiger192));
    require(first.parameters[0] != second.parameters[0]); deterministic = true; require(scrubbed);
}
struct Source final : AuthenticationSource {
    std::string cid;
    Source() { TigerDigest pid{}; for (std::size_t i = 0; i < pid.size(); ++i) { pid[i] = static_cast<unsigned char>(i+7); } cid = tiger_base32(*tiger192(pid)); }
    AuthenticationPreparation prepare(std::string_view nick,std::string_view supplied,dc24h::users::Transport transport,PasswordChallenge& challenge) const override {
        if (nick != "RegisteredADC" && supplied != cid) { return GuestAuthentication{}; }
        if (nick != "RegisteredADC" || supplied != cid || transport != dc24h::users::Transport::adcs) { return DeniedAuthentication{}; }
        return PasswordAuthentication{42,std::get<Message>(challenge.issue(password,SessionHash::tiger192))};
    }
    bool allows_identity(std::string_view nick,std::string_view supplied,std::optional<std::uint64_t> account) const noexcept override {
        return account ? *account == 42 && nick == "RegisteredADC" && supplied == cid : nick != "RegisteredADC" && supplied != cid;
    }
};
std::string drain(LoginHub& hub,std::uint64_t id) {
    std::string output;
    while (!hub.output(id).empty()) { const auto wire = hub.output(id); output += wire; (void)hub.consume(id,wire.size()); }
    return output;
}
std::string identify(LoginHub& hub,std::uint64_t id,dc24h::users::Transport transport) {
    hub.connect(id,"127.0.0.1",transport);
    if (transport == dc24h::users::Transport::adcs) { hub.transport_ready(id); }
    hub.receive(id,parse("HSUP ADBASE ADTIGR")); const auto output = drain(hub,id);
    const auto start = output.find("ISID "); require(start != std::string::npos);
    const auto sid = output.substr(start+5,4);
    TigerDigest pid{}; for (std::size_t i = 0; i < pid.size(); ++i) { pid[i] = static_cast<unsigned char>(i+7); }
    hub.receive(id,parse("BINF "+sid+" NIRegisteredADC ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)+" I40.0.0.0"));
    return sid;
}
void login() {
    LoginLimits limits; limits.authentication = std::make_shared<Source>();
    { LoginHub hub(limits); const auto sid = identify(hub,1,dc24h::users::Transport::adcs);
      require(hub.state(1) == ConnectionState::verify && hub.users() == 0 && !hub.authenticated_account(1));
      const auto wire = hub.output(1); require(wire.starts_with("IGPA "));
      (void)hub.consume(1,wire.size()-1); hub.receive(1,parse("HPAS "+std::string(response)));
      require(hub.closing(1) && hub.users() == 0); require(drain(hub,1).find("IQUI "+sid) != std::string::npos); }
    { LoginHub hub(limits); const auto sid = identify(hub,1,dc24h::users::Transport::adcs);
      require(drain(hub,1).starts_with("IGPA "));
      hub.receive(1,parse("HPAS "+std::string(response)));
      require(!hub.ready(1) && !hub.authenticated_account(1));
      require(hub.output(1).starts_with("IINF ")); (void)hub.consume(1,hub.output(1).size());
      require(hub.output(1).starts_with("BINF "+sid));
      (void)hub.consume(1,hub.output(1).size()-1); require(!hub.authenticated_account(1));
      require(hub.consume(1,1) && hub.authenticated_account(1) == 42); (void)drain(hub,1);
      hub.receive(1,parse("BINF "+sid+" NIRenamed")); require(hub.closing(1)); }
    { LoginHub hub(limits); (void)identify(hub,1,dc24h::users::Transport::adcs); (void)drain(hub,1);
      hub.receive(1,parse("HPAS "+std::string(39,'A'))); require(hub.closing(1) && hub.users() == 0);
      const auto terminal = drain(hub,1); require(terminal.find("ISTA 223 ") != std::string::npos);
      require(terminal.find(password) == std::string::npos && terminal.find(response) == std::string::npos); }
    { LoginHub hub(limits); (void)identify(hub,1,dc24h::users::Transport::adc);
      require(hub.closing(1) && drain(hub,1).starts_with("ISTA 223 ")); }
    require(scrubbed);
}
}
int main() {
    try { codec(); login(); std::cout << "ADC challenge and login checks passed\n"; }
    catch (const std::exception& error) { fail_after = -1; std::cerr << error.what() << '\n'; return 1; }
}
