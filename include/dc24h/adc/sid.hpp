#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace dc24h::adc {

inline constexpr std::uint32_t max_sid_value = (1U << 20U) - 1U;

// A protocol value, not an ownership token or an authentication credential.
class Sid final {
public:
    [[nodiscard]] static std::optional<Sid> from_value(std::uint32_t value) noexcept;
    [[nodiscard]] static std::optional<Sid> parse(std::string_view text) noexcept;
    [[nodiscard]] std::uint32_t value() const noexcept { return value_; }
    [[nodiscard]] std::array<char, 4> encode() const noexcept;
    [[nodiscard]] std::string str() const;
    friend bool operator==(Sid, Sid) = default;
private:
    explicit Sid(std::uint32_t value) noexcept : value_(value) {}
    std::uint32_t value_;
};

namespace detail { struct SidPoolState; }

// Move-only unique reservation. Reset/destruction releases exactly once, even
// after the originating SidPool is destroyed. Confined to the pool's thread.
class SidLease final {
public:
    SidLease() noexcept = default;
    ~SidLease();
    SidLease(const SidLease&) = delete;
    SidLease& operator=(const SidLease&) = delete;
    SidLease(SidLease&& other) noexcept;
    SidLease& operator=(SidLease&& other) noexcept;
    [[nodiscard]] explicit operator bool() const noexcept { return sid_.has_value(); }
    [[nodiscard]] std::optional<Sid> sid() const noexcept { return sid_; }
    void reset() noexcept;
private:
    friend class SidPool;
    SidLease(std::shared_ptr<detail::SidPoolState> state, Sid sid) noexcept;
    std::shared_ptr<detail::SidPoolState> state_;
    std::optional<Sid> sid_;
};

// One pool per hub, shared by all its ADC/ADCS sessions on one owning thread.
// Local policy reserves zero/AAAA; clients receive values 1..capacity.
class SidPool final {
public:
    explicit SidPool(std::uint32_t capacity = max_sid_value);
    SidPool(const SidPool&) = delete;
    SidPool& operator=(const SidPool&) = delete;
    SidPool(SidPool&&) = delete;
    SidPool& operator=(SidPool&&) = delete;
    // Empty lease on exhaustion. No allocation after successful construction.
    [[nodiscard]] SidLease acquire() noexcept;
    [[nodiscard]] bool in_use(Sid sid) const noexcept;
    [[nodiscard]] std::uint32_t capacity() const noexcept;
    [[nodiscard]] std::uint32_t size() const noexcept;
private:
    std::shared_ptr<detail::SidPoolState> state_;
};

} // namespace dc24h::adc
