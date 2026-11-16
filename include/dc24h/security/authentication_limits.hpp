#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace dc24h::security {
struct AuthenticationLimits {
    std::uint32_t attempts_per_ip{20}, attempts_per_account{10}, attempts_per_second{128};
    std::chrono::seconds window{60};
    std::size_t max_tracked_ips{4096}, max_tracked_accounts{4096};
};
void validate(const AuthenticationLimits& limits);
class AuthenticationAddress final {
public:
    [[nodiscard]] static std::optional<AuthenticationAddress> parse(std::string_view numeric) noexcept;
    friend bool operator==(const AuthenticationAddress&, const AuthenticationAddress&) = default;
private:
    friend struct AuthenticationAddressHash;
    explicit AuthenticationAddress(std::array<unsigned char,17> bytes) noexcept : bytes_(bytes) {}
    std::array<unsigned char,17> bytes_;
};
struct AuthenticationAddressHash {
    [[nodiscard]] std::size_t operator()(const AuthenticationAddress& address) const noexcept;
};
// Single owner-thread, bounded token buckets, supplied monotonic time. Entries
// survive socket cleanup; accepted attempts debit tokens without success refunds.
// Account keys must come from trusted authentication policy, never client fields.
class AuthenticationLimiter final {
public:
    using Clock = std::chrono::steady_clock;
    explicit AuthenticationLimiter(AuthenticationLimits limits = {});
    [[nodiscard]] bool admit_ip(AuthenticationAddress peer, Clock::time_point now);
    [[nodiscard]] bool admit_account(std::uint64_t account, Clock::time_point now);
    [[nodiscard]] std::size_t tracked_ips() const noexcept { return ips_.size(); }
    [[nodiscard]] std::size_t tracked_accounts() const noexcept { return accounts_.size(); }
private:
    struct Entry { std::uint64_t credits; Clock::time_point updated, admitted; };
    [[nodiscard]] Clock::time_point observe(Clock::time_point now) noexcept;
    void prune(Clock::time_point now) noexcept;
    static Entry fresh(std::uint32_t count, std::chrono::seconds window, Clock::time_point now) noexcept;
    static void refill(Entry& entry, std::uint32_t count, std::chrono::seconds window, Clock::time_point now) noexcept;
    AuthenticationLimits limits_;
    std::optional<Clock::time_point> last_, last_prune_;
    std::optional<Entry> global_;
    std::unordered_map<AuthenticationAddress,Entry,AuthenticationAddressHash> ips_;
    std::unordered_map<std::uint64_t,Entry> accounts_;
};
} // namespace dc24h::security
