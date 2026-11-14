#pragma once

#include "dc24h/users/session.hpp"
#include <cstddef>
#include <cstdint>

namespace dc24h::transfers {
inline constexpr std::uint32_t max_request_rate = 1000000;
inline constexpr std::uint32_t max_transfer_byte_rate = 16 * 1024 * 1024;
struct FloodLimits {
    std::uint32_t rate, burst, byte_rate, byte_burst;
};
struct Limits {
    FloodLimits requests{32,64,131072,262144};
    FloodLimits incoming{128,256,524288,1048576};
};
void validate_limits(const Limits& limits);
enum class Direction { outgoing, incoming };
// Owner-thread, fixed memory per connection lifetime. CTM/RCM, D/E, protocols,
// tokens and recipients share budgets. Count/bytes debit together; rejected
// attempts debit neither. No clock reads, allocations, identity or payload state.
class TransferGuard final {
public:
    explicit TransferGuard(Limits limits = {});
    [[nodiscard]] bool admit(Direction direction, std::size_t decoded_bytes, users::TimePoint now) noexcept;
    // Shared by all transfer-limit notices; success and permission/INF changes
    // never reset it. No notice is sent to the target or echoed to the source.
    [[nodiscard]] bool notice(users::TimePoint now) noexcept;
private:
    struct Bucket { FloodLimits limits; std::uint64_t count, bytes; users::TimePoint updated; };
    [[nodiscard]] users::TimePoint observe(users::TimePoint now) noexcept;
    Bucket outgoing_, incoming_;
    std::optional<users::TimePoint> updated_, notice_;
};
} // namespace dc24h::transfers
