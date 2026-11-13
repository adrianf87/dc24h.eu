#pragma once

#include "dc24h/search/tokens.hpp"
#include <cstdint>

namespace dc24h::search {
inline constexpr std::uint32_t max_operation_rate = 1000000;
inline constexpr std::uint32_t max_security_byte_rate = 16 * 1024 * 1024;
struct FloodLimits {
    std::uint32_t rate, burst, byte_rate, byte_burst;
};
struct Permissions {
    bool queries{true}, results{true};
    friend bool operator==(const Permissions&, const Permissions&) = default;
};
struct SecuritySettings {
    bool queries_enabled{true}, results_enabled{true};
    Permissions guests{};
    bool require_result_token{false};
    TokenLimits tokens{};
    FloodLimits queries{10,32,32768,131072};
    FloodLimits results{64,128,262144,1048576};
};
void validate_security(const SecuritySettings& settings);
enum class Operation { query, result, incoming_result };
enum class PermissionError { disabled, forbidden };
[[nodiscard]] std::optional<PermissionError> authorize(const SecuritySettings& settings,
    Permissions permissions, Operation operation) noexcept;
// One owner-thread guard per connection lifetime. Query, sent-result and received-
// result count/byte buckets are independent; count/bytes debit atomically. No
// payload/history/identity is retained and no call allocates or reads the clock.
class SearchGuard final {
public:
    explicit SearchGuard(const SecuritySettings& settings = {});
    [[nodiscard]] bool admit(Operation operation, std::size_t decoded_bytes, users::TimePoint now) noexcept;
    // Shared one-per-second notice allowance for policy/flood denials. Successful
    // traffic, permission changes and operation switching do not reset it.
    [[nodiscard]] bool notice(users::TimePoint now) noexcept;
private:
    struct Bucket { FloodLimits limits; std::uint64_t count, bytes; users::TimePoint updated; };
    [[nodiscard]] users::TimePoint observe(users::TimePoint now) noexcept;
    Bucket queries_, results_, received_;
    std::optional<users::TimePoint> updated_, notice_;
};
} // namespace dc24h::search
