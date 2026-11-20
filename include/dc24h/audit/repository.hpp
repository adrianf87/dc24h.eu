#pragma once
#include "dc24h/database/database.hpp"
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

namespace dc24h::audit {
inline constexpr std::size_t page_size = 16;
enum class ErrorKind { invalid_input, schema, invalid_record };
class Error final : public std::exception {
public:
    explicit Error(ErrorKind kind) noexcept : kind_(kind) {}
    [[nodiscard]] ErrorKind kind() const noexcept { return kind_; }
    [[nodiscard]] const char* what() const noexcept override;
private:
    ErrorKind kind_;
};
// Fixed successful maintenance facts. No password, proof, nickname or reason text.
struct Event {
    std::uint64_t id{};
    std::int64_t occurred_at{};
    std::string action;
    std::uint64_t actor_id{};
    std::string target_kind;
    std::uint64_t target_id{};
    std::string outcome{"succeeded"};
    friend bool operator==(const Event&, const Event&) = default;
};
[[nodiscard]] bool known_action(std::string_view action) noexcept;
// Append-only history. Reads and schema checks refuse an open transaction.
// append_in_transaction must run on the same transaction as the change it records.
class Repository final {
public:
    explicit Repository(database::Connection& connection) noexcept : connection_(connection) {}
    void require_schema();
    void append_in_transaction(std::string_view action, std::uint64_t actor, std::string_view target_kind,
        std::uint64_t target, std::int64_t occurred_at);
    // Durable intent precedes a filesystem mutation. A crash leaves a
    // 'started' entry, never a misleading successful record.
    [[nodiscard]] std::uint64_t begin_configuration(std::string_view action, std::uint64_t actor,
        std::int64_t occurred_at);
    void finish_configuration(std::uint64_t event_id, std::uint64_t actor, bool succeeded);
    // Append an intent before any systemd operation; a crash or ambiguous
    // result intentionally retains the 'started' row for manual recovery.
    [[nodiscard]] std::uint64_t begin_service(std::string_view action, std::uint64_t actor, std::int64_t occurred_at);
    void finish_service(std::uint64_t event_id, std::uint64_t actor, bool succeeded);
    [[nodiscard]] std::vector<Event> list(std::uint64_t after);
private:
    database::Connection& connection_;
};
} // namespace dc24h::audit
