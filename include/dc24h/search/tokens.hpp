#pragma once

#include "dc24h/search/query.hpp"
#include "dc24h/users/session_manager.hpp"

#include <chrono>
#include <map>
#include <memory>
#include <string_view>
#include <utility>

namespace dc24h::search {
struct TokenLimits {
    std::size_t total{4096}, per_owner{8};
    std::chrono::seconds lifetime{60};
};
enum class TokenError { missing_token, invalid_token, missing_owner, owner_not_ready, duplicate_token,
                        total_limit, owner_limit, id_exhausted, time_exhausted, inactive_search };
struct ActiveSearch {
    std::uint64_t id; // Local nonzero generation; never a credential or global ID.
    users::SessionId owner;
    users::TimePoint expires;
    friend bool operator==(const ActiveSearch&, const ActiveSearch&) = default;
};
namespace detail { struct TokenDomain final {}; }
class TokenHandle final {
public:
    [[nodiscard]] const ActiveSearch& search() const noexcept { return search_; }
    [[nodiscard]] std::string_view token() const noexcept { return token_; }
private:
    friend class Tokens;
    TokenHandle(std::shared_ptr<const detail::TokenDomain> domain, ActiveSearch search, std::string token)
        : domain_(std::move(domain)), search_(search), token_(std::move(token)) {}
    std::shared_ptr<const detail::TokenDomain> domain_;
    ActiveSearch search_;
    std::string token_;
};
// Owner-thread registry, borrowing SessionManager. No sockets, automatic clocks,
// query payloads, result routing or logging. Handles do not borrow registry data
// and do not cancel on destruction. The caller completes or prunes searches.
class Tokens final {
public:
    // first_id seeds a fresh domain's ID stream; exhaustion never wraps/reuses.
    explicit Tokens(const users::SessionManager& sessions, TokenLimits limits = {}, std::uint64_t first_id = 1);
    Tokens(const Tokens&) = delete;
    Tokens& operator=(const Tokens&) = delete;
    Tokens(Tokens&&) = delete;
    Tokens& operator=(Tokens&&) = delete;
    [[nodiscard]] std::variant<TokenHandle, TokenError> activate(users::SessionId owner, std::string_view token, users::TimePoint now);
    // Exact session-lifetime + decoded-token lookup; no Unicode/case rewriting.
    // Lookup, cancellation and maintenance never allocate or throw.
    [[nodiscard]] std::optional<ActiveSearch> find(users::SessionId owner, std::string_view token, users::TimePoint now) noexcept;
    [[nodiscard]] bool active(const TokenHandle& handle, users::TimePoint now) noexcept;
    [[nodiscard]] bool cancel(const TokenHandle& handle) noexcept;
    std::size_t forget(users::SessionId owner) noexcept;
    std::size_t prune(users::TimePoint now) noexcept;
    // Stored entries, possibly awaiting cleanup after a session closes.
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
private:
    struct Key { users::SessionId owner; std::string token; };
    struct View { users::SessionId owner; std::string_view token; };
    struct Less {
        using is_transparent = void;
        bool operator()(const Key& a, const Key& b) const noexcept;
        bool operator()(const Key& a, const View& b) const noexcept;
        bool operator()(const View& a, const Key& b) const noexcept;
    };
    using Deadlines = std::multimap<users::TimePoint, const Key*>;
    struct Entry { ActiveSearch search; Deadlines::iterator deadline; };
    using Entries = std::map<Key, Entry, Less>;
    [[nodiscard]] users::TimePoint observe(users::TimePoint now) noexcept;
    std::size_t expire(users::TimePoint now) noexcept;
    Entries::iterator erase(Entries::iterator it) noexcept;
    const users::SessionManager& sessions_;
    TokenLimits limits_;
    std::uint64_t next_id_;
    bool exhausted_{false};
    users::TimePoint last_time_{users::TimePoint::min()};
    const std::shared_ptr<const detail::TokenDomain> domain_{std::make_shared<const detail::TokenDomain>()};
    Entries entries_;
    Deadlines deadlines_;
};
} // namespace dc24h::search
