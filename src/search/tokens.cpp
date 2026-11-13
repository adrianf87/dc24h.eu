#include "dc24h/search/tokens.hpp"
#include "dc24h/text/public_text.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace dc24h::search {
namespace {
bool before(users::SessionId a, std::string_view at, users::SessionId b, std::string_view bt) noexcept {
    return a.value() < b.value() || (a == b && at < bt);
}
} // namespace
bool Tokens::Less::operator()(const Key& a, const Key& b) const noexcept { return before(a.owner,a.token,b.owner,b.token); }
bool Tokens::Less::operator()(const Key& a, const View& b) const noexcept { return before(a.owner,a.token,b.owner,b.token); }
bool Tokens::Less::operator()(const View& a, const Key& b) const noexcept { return before(a.owner,a.token,b.owner,b.token); }
void validate_token_limits(const TokenLimits& limits) {
    if (!limits.total || limits.total > 65535 || !limits.per_owner || limits.per_owner > 64 || limits.per_owner > limits.total ||
        limits.lifetime < std::chrono::seconds{1} || limits.lifetime > std::chrono::hours{1}) {
        throw std::invalid_argument("Invalid search token limits");
    }
}
Tokens::Tokens(const users::SessionManager& sessions, TokenLimits limits, std::uint64_t first_id)
    : sessions_(sessions), limits_(limits), next_id_(first_id) {
    validate_token_limits(limits);
    if (!first_id) { throw std::invalid_argument("Invalid search token ID seed"); }
}
users::TimePoint Tokens::observe(users::TimePoint now) noexcept {
    last_time_ = std::max(now,last_time_); return last_time_;
}
Tokens::Entries::iterator Tokens::erase(Entries::iterator it) noexcept {
    deadlines_.erase(it->second.deadline);
    return entries_.erase(it);
}
std::size_t Tokens::expire(users::TimePoint now) noexcept {
    now = observe(now); std::size_t count = 0;
    while (!deadlines_.empty() && deadlines_.begin()->first <= now) {
        // std::map keeps key addresses stable until the matching record is erased.
        const auto it = entries_.find(*deadlines_.begin()->second);
        (void)erase(it); ++count;
    }
    return count;
}
std::size_t Tokens::forget(users::SessionId owner) noexcept {
    auto it = entries_.lower_bound(View{owner,{}}); std::size_t count = 0;
    while (it != entries_.end() && it->first.owner == owner) { it = erase(it); ++count; }
    return count;
}
std::size_t Tokens::prune(users::TimePoint now) noexcept {
    auto count = expire(now);
    for (auto it = entries_.begin(); it != entries_.end();) {
        const auto* model = sessions_.find(it->first.owner);
        if (!model || !model->ready()) { it = erase(it); ++count; }
        else { ++it; }
    }
    return count;
}
std::variant<TokenHandle, TokenError> Tokens::activate(users::SessionId owner, std::string_view token, users::TimePoint now) {
    const auto* model = sessions_.find(owner);
    if (!model) { return TokenError::missing_owner; }
    if (!model->ready()) { return TokenError::owner_not_ready; }
    if (token.empty() || token.size() > max_token_bytes || !text::valid_public_text(token)) { return TokenError::invalid_token; }
    (void)expire(now); now = last_time_;
    if (entries_.find(View{owner,token}) != entries_.end()) { return TokenError::duplicate_token; }
    if (entries_.size() == limits_.total) { (void)prune(now); }
    if (entries_.size() == limits_.total) { return TokenError::total_limit; }
    std::size_t owned = 0;
    for (auto it = entries_.lower_bound(View{owner,{}}); it != entries_.end() && it->first.owner == owner; ++it) { ++owned; }
    if (owned == limits_.per_owner) { return TokenError::owner_limit; }
    if (exhausted_) { return TokenError::id_exhausted; }
    const auto lifetime = std::chrono::duration_cast<users::Clock::duration>(limits_.lifetime);
    const auto end = now > users::TimePoint::max()-lifetime ? users::TimePoint::max() : now+lifetime;
    if (end <= now) { return TokenError::time_exhausted; }
    const ActiveSearch search{next_id_,owner,end};
    TokenHandle handle(domain_,search,std::string(token));
    const auto [it, inserted] = entries_.emplace(Key{owner,std::string(token)}, Entry{search,{}});
    if (!inserted) { return TokenError::duplicate_token; }
    try { it->second.deadline = deadlines_.emplace(end,&it->first); }
    catch (...) { entries_.erase(it); throw; }
    // No allocation remains after both indexes commit; failures consume no ID.
    if (next_id_ == std::numeric_limits<std::uint64_t>::max()) { exhausted_ = true; }
    else { ++next_id_; }
    return std::variant<TokenHandle, TokenError>{std::in_place_type<TokenHandle>,std::move(handle)};
}
std::optional<ActiveSearch> Tokens::find(users::SessionId owner, std::string_view token, users::TimePoint now) noexcept {
    (void)expire(now);
    const auto* model = sessions_.find(owner);
    if (!model || !model->ready()) { (void)forget(owner); return {}; }
    if (token.empty() || token.size() > max_token_bytes) { return {}; }
    const auto it = entries_.find(View{owner,token});
    return it == entries_.end() ? std::optional<ActiveSearch>{} : it->second.search;
}
bool Tokens::active(const TokenHandle& handle, users::TimePoint now) noexcept {
    if (handle.domain_ != domain_) { return false; }
    const auto found = find(handle.search_.owner,handle.token_,now);
    return found && found->id == handle.search_.id;
}
bool Tokens::cancel(const TokenHandle& handle) noexcept {
    if (handle.domain_ != domain_) { return false; }
    const auto it = entries_.find(View{handle.search_.owner,handle.token_});
    if (it == entries_.end() || it->second.search.id != handle.search_.id) { return false; }
    (void)erase(it); return true;
}
} // namespace dc24h::search
