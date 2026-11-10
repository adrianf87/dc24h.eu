#pragma once

#include "dc24h/users/session.hpp"

#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace dc24h::users {
enum class RegistryError { stale_id, capacity_reached, missing, not_closed };
using Registration = std::variant<Session*, RegistryError>;
struct SessionCounts {
    std::size_t total{}, accepted{}, negotiating{}, identified{}, ready{}, closing{}, closed{};
    friend bool operator==(const SessionCounts&, const SessionCounts&) = default;
};

// One owner/thread. Owns models, never sockets, protocol peers or credentials.
// References survive insertions/removals of OTHER IDs; removal invalidates them.
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
    [[nodiscard]] Session* find(SessionId id) noexcept;
    [[nodiscard]] const Session* find(SessionId id) const noexcept;
    // Cleanup owner must finalize the model before removal. No implicit close.
    [[nodiscard]] std::optional<RegistryError> remove(SessionId id) noexcept;
    [[nodiscard]] std::optional<Snapshot> snapshot(SessionId id) const;
    // Strictly increasing IDs after an exclusive cursor; no live iterators escape.
    // Independent point-in-time pages, not a frozen multi-page transaction.
    [[nodiscard]] std::vector<Snapshot> page(std::uint64_t after = 0, std::size_t limit = 64) const;
    [[nodiscard]] SessionCounts counts() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return sessions_.size(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
private:
    std::size_t capacity_;
    std::uint64_t greatest_id_{};
    std::map<std::uint64_t, std::unique_ptr<Session>> sessions_;
};
} // namespace dc24h::users
