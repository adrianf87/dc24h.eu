#include "dc24h/security/authentication_limits.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace dc24h::security {
namespace {
using Clock = AuthenticationLimiter::Clock;
std::uint64_t units(std::chrono::seconds window) noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(window).count());
}
std::uint64_t elapsed(Clock::time_point from, Clock::time_point to, std::chrono::seconds horizon) noexcept {
    if (to <= from) { return 0; }
    using Rep = Clock::duration::rep;
    using Unsigned = std::make_unsigned_t<Rep>;
    const auto ticks = static_cast<Unsigned>(to.time_since_epoch().count())-static_cast<Unsigned>(from.time_since_epoch().count());
    const auto bound = std::chrono::duration_cast<Clock::duration>(horizon).count();
    if (ticks >= static_cast<Unsigned>(bound)) { return units(horizon); }
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::duration(static_cast<Rep>(ticks))).count());
}
}
void validate(const AuthenticationLimits& limits) {
    if (!limits.attempts_per_ip || limits.attempts_per_ip > 100000 ||
        !limits.attempts_per_account || limits.attempts_per_account > 100000 ||
        !limits.attempts_per_second || limits.attempts_per_second > 100000 ||
        limits.window < std::chrono::seconds(1) || limits.window > std::chrono::hours(1) ||
        !limits.max_tracked_ips || limits.max_tracked_ips > 65536 ||
        !limits.max_tracked_accounts || limits.max_tracked_accounts > 65536) {
        throw std::invalid_argument("Invalid authentication attempt limits");
    }
}
std::optional<AuthenticationAddress> AuthenticationAddress::parse(std::string_view numeric) noexcept {
    if (numeric.empty() || numeric.size() >= INET6_ADDRSTRLEN || numeric.find('\0') != std::string_view::npos) { return {}; }
    std::array<char,INET6_ADDRSTRLEN> text{}; std::copy(numeric.begin(),numeric.end(),text.begin());
    std::array<unsigned char,17> key{}; in_addr v4{}; in6_addr v6{};
    if (::inet_pton(AF_INET,text.data(),&v4) == 1) {
        key[0] = 4; const auto* bytes = reinterpret_cast<const unsigned char*>(&v4);
        std::copy_n(bytes,4,key.begin()+13);
    } else if (::inet_pton(AF_INET6,text.data(),&v6) == 1) {
        if (IN6_IS_ADDR_V4MAPPED(&v6)) { key[0] = 4; std::copy_n(v6.s6_addr+12,4,key.begin()+13); }
        else { key[0] = 6; std::copy_n(v6.s6_addr,16,key.begin()+1); }
    } else { return {}; }
    return AuthenticationAddress(key);
}
std::size_t AuthenticationAddressHash::operator()(const AuthenticationAddress& address) const noexcept {
    auto hash = static_cast<std::size_t>(1469598103934665603ULL);
    for (auto byte : address.bytes_) { hash ^= byte; hash *= static_cast<std::size_t>(1099511628211ULL); }
    return hash;
}
AuthenticationLimiter::AuthenticationLimiter(AuthenticationLimits limits) : limits_(limits) { validate(limits_); }
AuthenticationLimiter::Clock::time_point AuthenticationLimiter::observe(Clock::time_point now) noexcept {
    if (!last_ || now > *last_) { last_ = now; }
    return *last_;
}
AuthenticationLimiter::Entry AuthenticationLimiter::fresh(std::uint32_t count,std::chrono::seconds window,Clock::time_point now) noexcept {
    return {units(window)*count,now,now};
}
void AuthenticationLimiter::refill(Entry& entry,std::uint32_t count,std::chrono::seconds window,Clock::time_point now) noexcept {
    const auto capacity = units(window)*count;
    entry.credits += std::min(capacity-entry.credits,elapsed(entry.updated,now,window)*count);
    entry.updated = now;
}
void AuthenticationLimiter::prune(Clock::time_point now) noexcept {
    if (last_prune_ && elapsed(*last_prune_,now,std::chrono::seconds(1)) < units(std::chrono::seconds(1))) { return; }
    last_prune_ = now;
    const auto expired = [&](const Entry& entry) { return elapsed(entry.admitted,now,limits_.window) >= units(limits_.window); };
    for (auto it = ips_.begin(); it != ips_.end();) { if (expired(it->second)) { it = ips_.erase(it); } else { ++it; } }
    for (auto it = accounts_.begin(); it != accounts_.end();) { if (expired(it->second)) { it = accounts_.erase(it); } else { ++it; } }
}
bool AuthenticationLimiter::admit_ip(AuthenticationAddress peer,Clock::time_point now) {
    now = observe(now); prune(now);
    constexpr auto second = std::chrono::seconds(1);
    if (!global_) { global_ = fresh(limits_.attempts_per_second,second,now); }
    refill(*global_,limits_.attempts_per_second,second,now);
    if (global_->credits < units(second)) { return false; }
    auto found = ips_.find(peer);
    if (found == ips_.end()) {
        if (ips_.size() >= limits_.max_tracked_ips) { return false; }
        found = ips_.emplace(peer,fresh(limits_.attempts_per_ip,limits_.window,now)).first;
    } else { refill(found->second,limits_.attempts_per_ip,limits_.window,now); }
    if (found->second.credits < units(limits_.window)) { return false; }
    found->second.credits -= units(limits_.window); found->second.admitted = now;
    global_->credits -= units(second); global_->admitted = now;
    return true;
}
bool AuthenticationLimiter::admit_account(std::uint64_t account,Clock::time_point now) {
    if (!account || account > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) { return false; }
    now = observe(now); prune(now);
    auto found = accounts_.find(account);
    if (found == accounts_.end()) {
        if (accounts_.size() >= limits_.max_tracked_accounts) { return false; }
        found = accounts_.emplace(account,fresh(limits_.attempts_per_account,limits_.window,now)).first;
    } else { refill(found->second,limits_.attempts_per_account,limits_.window,now); }
    if (found->second.credits < units(limits_.window)) { return false; }
    found->second.credits -= units(limits_.window); found->second.admitted = now;
    return true;
}
} // namespace dc24h::security
