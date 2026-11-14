#include "dc24h/transfers/limits.hpp"
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace dc24h::transfers {
namespace {
constexpr std::uint64_t scale = 1000000000;
void validate_flood(const FloodLimits& limits) {
    if (!limits.rate || limits.rate > max_request_rate || !limits.burst || limits.burst > max_request_rate ||
        !limits.byte_rate || limits.byte_rate > max_transfer_byte_rate ||
        limits.byte_burst < 16384 || limits.byte_burst > max_transfer_byte_rate) {
        throw std::invalid_argument("Invalid transfer anti-flood limits");
    }
}
std::uint64_t elapsed(users::TimePoint from, users::TimePoint to) noexcept {
    if (to <= from) { return 0; }
    using Rep = users::Clock::duration::rep;
    using Unsigned = std::make_unsigned_t<Rep>;
    const auto ticks = static_cast<Unsigned>(to.time_since_epoch().count()) - static_cast<Unsigned>(from.time_since_epoch().count());
    const auto horizon = std::chrono::duration_cast<users::Clock::duration>(std::chrono::seconds(max_transfer_byte_rate));
    if (ticks >= static_cast<Unsigned>(horizon.count())) { return static_cast<std::uint64_t>(max_transfer_byte_rate) * scale; }
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(users::Clock::duration(static_cast<Rep>(ticks))).count());
}
void refill(std::uint64_t& credit, std::uint32_t capacity, std::uint32_t rate, std::uint64_t nanoseconds) noexcept {
    const auto missing = static_cast<std::uint64_t>(capacity) * scale - credit;
    if (nanoseconds >= (missing + rate - 1) / rate) { credit += missing; }
    else { credit += nanoseconds * rate; }
}
} // namespace
void validate_limits(const Limits& limits) {
    validate_flood(limits.requests); validate_flood(limits.incoming);
}
TransferGuard::TransferGuard(Limits limits)
    : outgoing_{limits.requests,static_cast<std::uint64_t>(limits.requests.burst)*scale,static_cast<std::uint64_t>(limits.requests.byte_burst)*scale,{}},
      incoming_{limits.incoming,static_cast<std::uint64_t>(limits.incoming.burst)*scale,static_cast<std::uint64_t>(limits.incoming.byte_burst)*scale,{}} { validate_limits(limits); }
users::TimePoint TransferGuard::observe(users::TimePoint now) noexcept {
    if (updated_) { now = std::max(now,*updated_); }
    else { outgoing_.updated = incoming_.updated = now; }
    updated_ = now; return now;
}
bool TransferGuard::admit(Direction direction, std::size_t decoded_bytes, users::TimePoint now) noexcept {
    auto* bucket = direction == Direction::outgoing ? &outgoing_ : direction == Direction::incoming ? &incoming_ : nullptr;
    if (!bucket) { return false; }
    now = observe(now); const auto delta = elapsed(bucket->updated,now); bucket->updated = now;
    refill(bucket->count,bucket->limits.burst,bucket->limits.rate,delta);
    refill(bucket->bytes,bucket->limits.byte_burst,bucket->limits.byte_rate,delta);
    if (decoded_bytes > bucket->limits.byte_burst || bucket->count < scale || bucket->bytes < decoded_bytes*scale) { return false; }
    bucket->count -= scale; bucket->bytes -= decoded_bytes*scale; return true;
}
bool TransferGuard::notice(users::TimePoint now) noexcept {
    now = observe(now);
    if (notice_ && elapsed(*notice_,now) < scale) { return false; }
    notice_ = now; return true;
}
} // namespace dc24h::transfers
