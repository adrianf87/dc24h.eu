#pragma once

#include "dc24h/users/session.hpp"

#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace dc24h::users {
enum class RegistryError {
    stale_id, capacity_reached, missing, not_closed,
    duplicate_sid, duplicate_cid, duplicate_nickname
};
using Registration = std::variant<const Session*, RegistryError>;
using MutationResult = std::optional<std::variant<RegistryError, SessionError>>;
struct SessionCounts {
    std::size_t total{}, accepted{}, negotiating{}, identified{}, ready{}, closing{}, closed{};
    friend bool operator==(const SessionCounts&, const SessionCounts&) = default;
};

// One owner/thread. Models and public identity indexes have one owner. Borrowed
// references are const; all managed mutations pass through this registry.
// References survive operations on OTHER IDs; removal invalidates them.
class SessionManager final {
public:
    static constexpr std::size_t max_capacity = 65535;
    static constexpr std::size_t max_page_size = 256;
    explicit SessionManager(std::size_t capacity);
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    SessionManager(SessionManager&&) = delete;
    SessionManager& operator=(SessionManager&&) = delete;
    [[nodiscard]] Registration add(SessionId id, Transport transport, TimePoint accepted);
    [[nodiscard]] const Session* find(SessionId id) const noexcept;
    // Exact, case-sensitive public keys; includes identified/ready/closing and
    // finalized entries until remove. Check ready() before application routing.
    [[nodiscard]] const Session* by_sid(std::string_view sid) const noexcept;
    [[nodiscard]] const Session* by_cid(std::string_view cid) const noexcept;
    [[nodiscard]] const Session* by_nickname(std::string_view nickname) const noexcept;
    [[nodiscard]] MutationResult start_negotiation(SessionId id, TimePoint now) noexcept;
    [[nodiscard]] MutationResult identify(SessionId id, User user, TimePoint now);
    [[nodiscard]] MutationResult update_user(SessionId id, User user, TimePoint now);
    [[nodiscard]] MutationResult rename(SessionId id, std::string_view nickname, TimePoint now);
    [[nodiscard]] MutationResult mark_ready(SessionId id, TimePoint now) noexcept;
    [[nodiscard]] MutationResult begin_close(SessionId id, CloseCause cause, TimePoint now) noexcept;
    [[nodiscard]] MutationResult force_close(SessionId id, CloseCause cause, TimePoint now) noexcept;
    [[nodiscard]] MutationResult finish_close(SessionId id, CloseCause cause, TimePoint now) noexcept;
    // Cleanup owner finalizes the model before removal. Index reservations stay
    // until removal; no implicit close or early nickname/CID reuse during drain.
    [[nodiscard]] std::optional<RegistryError> remove(SessionId id) noexcept;
    [[nodiscard]] std::optional<Snapshot> snapshot(SessionId id) const;
    // Strictly increasing IDs after an exclusive cursor; no live iterators escape.
    // Independent point-in-time pages, not a frozen multi-page transaction.
    [[nodiscard]] std::vector<Snapshot> page(std::uint64_t after = 0, std::size_t limit = 64) const;
    [[nodiscard]] SessionCounts counts() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return sessions_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t identified_users() const noexcept { return nicknames_.size(); }
private:
    using Index = std::map<std::string, SessionId, std::less<>>;
    [[nodiscard]] Session* mutable_find(SessionId id) noexcept;
    [[nodiscard]] const Session* indexed(const Index& index, std::string_view key, std::size_t maximum) const noexcept;
    void erase_identity(SessionId id, const Identity& identity) noexcept;
    std::size_t capacity_;
    std::uint64_t greatest_id_{};
    std::map<std::uint64_t, std::unique_ptr<Session>> sessions_;
    Index sids_, cids_, nicknames_;
};
} // namespace dc24h::users
