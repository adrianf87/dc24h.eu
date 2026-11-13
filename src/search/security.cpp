#include "dc24h/search/security.hpp"
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace dc24h::search {
namespace {
constexpr std::uint64_t scale = 1000000000;
void validate_flood(const FloodLimits& limits) {
    if (!limits.rate || limits.rate > max_operation_rate || !limits.burst || limits.burst > max_operation_rate ||
        !limits.byte_rate || limits.byte_rate > max_security_byte_rate ||
        limits.byte_burst < 16384 || limits.byte_burst > max_security_byte_rate) {
        throw std::invalid_argument("Invalid search anti-flood limits");
    }
}
std::uint64_t elapsed(users::TimePoint from, users::TimePoint to) noexcept {
    if (to <= from) { return 0; }
    using Rep = users::Clock::duration::rep;
    using Unsigned = std::make_unsigned_t<Rep>;
    const auto ticks = static_cast<Unsigned>(to.time_since_epoch().count()) - static_cast<Unsigned>(from.time_since_epoch().count());
    const auto horizon = std::chrono::duration_cast<users::Clock::duration>(std::chrono::seconds(max_security_byte_rate));
    if (ticks >= static_cast<Unsigned>(horizon.count())) { return static_cast<std::uint64_t>(max_security_byte_rate) * scale; }
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(users::Clock::duration(static_cast<Rep>(ticks))).count());
}
void refill(std::uint64_t& credit, std::uint32_t capacity, std::uint32_t rate, std::uint64_t nanoseconds) noexcept {
    const auto missing = static_cast<std::uint64_t>(capacity) * scale - credit;
    if (nanoseconds >= (missing + rate - 1) / rate) { credit += missing; }
    else { credit += nanoseconds * rate; }
}
} // namespace
void validate_security(const SecuritySettings& settings) {
    validate_flood(settings.queries); validate_flood(settings.results); validate_token_limits(settings.tokens);
}
std::optional<PermissionError> authorize(const SecuritySettings& settings, Permissions permissions, Operation operation) noexcept {
    if (operation == Operation::query) {
        if (!settings.queries_enabled) { return PermissionError::disabled; }
        if (!permissions.queries) { return PermissionError::forbidden; }
    } else if (operation == Operation::result) {
        if (!settings.results_enabled) { return PermissionError::disabled; }
        if (!permissions.results) { return PermissionError::forbidden; }
    } else { return PermissionError::forbidden; }
    return {};
}
SearchGuard::SearchGuard(const SecuritySettings& settings)
    : queries_{settings.queries,static_cast<std::uint64_t>(settings.queries.burst)*scale,static_cast<std::uint64_t>(settings.queries.byte_burst)*scale,{}},
      results_{settings.results,static_cast<std::uint64_t>(settings.results.burst)*scale,static_cast<std::uint64_t>(settings.results.byte_burst)*scale,{}},
      received_(results_) { validate_security(settings); }
users::TimePoint SearchGuard::observe(users::TimePoint now) noexcept {
    if (updated_) { now = std::max(now,*updated_); }
    else { queries_.updated = results_.updated = received_.updated = now; }
    updated_ = now; return now;
}
bool SearchGuard::admit(Operation operation, std::size_t decoded_bytes, users::TimePoint now) noexcept {
    auto* bucket = operation == Operation::query ? &queries_ : operation == Operation::result ? &results_ :
                   operation == Operation::incoming_result ? &received_ : nullptr;
    if (!bucket) { return false; }
    now = observe(now); const auto delta = elapsed(bucket->updated,now); bucket->updated = now;
    refill(bucket->count,bucket->limits.burst,bucket->limits.rate,delta);
    refill(bucket->bytes,bucket->limits.byte_burst,bucket->limits.byte_rate,delta);
    if (decoded_bytes > bucket->limits.byte_burst || bucket->count < scale || bucket->bytes < decoded_bytes*scale) { return false; }
    bucket->count -= scale; bucket->bytes -= decoded_bytes*scale; return true;
}
bool SearchGuard::notice(users::TimePoint now) noexcept {
    now = observe(now);
    if (notice_ && elapsed(*notice_,now) < scale) { return false; }
    notice_ = now; return true;
}
} // namespace dc24h::search
