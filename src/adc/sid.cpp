#include "dc24h/adc/sid.hpp"

#include <bit>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dc24h::adc {
namespace {
constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
}

std::optional<Sid> Sid::from_value(std::uint32_t value) noexcept {
    if (value > max_sid_value) { return std::nullopt; }
    return Sid(value);
}

std::optional<Sid> Sid::parse(std::string_view text) noexcept {
    if (text.size() != 4) { return std::nullopt; }
    std::uint32_t value = 0;
    for (const char character : text) {
        const auto digit = alphabet.find(character);
        if (digit == std::string_view::npos) { return std::nullopt; }
        value = (value << 5U) | static_cast<std::uint32_t>(digit);
    }
    return Sid(value);
}

std::array<char, 4> Sid::encode() const noexcept {
    std::array<char, 4> result{};
    auto value = value_;
    for (std::size_t offset = result.size(); offset > 0; --offset) {
        result[offset - 1] = alphabet[value & 31U];
        value >>= 5U;
    }
    return result;
}

std::string Sid::str() const {
    const auto text = encode();
    return {text.data(), text.size()};
}

namespace detail {
struct SidPoolState {
    explicit SidPoolState(std::uint32_t count) : capacity(count), occupied((count + 63U) / 64U, 0) {}
    // Zero-based bitmap slots map to protocol values slot+1.
    std::uint32_t capacity;
    std::uint32_t active{0};
    std::uint32_t next{0};
    std::vector<std::uint64_t> occupied;

    std::optional<std::uint32_t> free_slot(std::uint32_t begin, std::uint32_t end) const noexcept {
        while (begin < end) {
            const auto word = begin / 64U;
            const auto offset = begin % 64U;
            auto mask = ~std::uint64_t{0} << offset;
            if (word == (end - 1U) / 64U && end % 64U != 0) {
                mask &= (std::uint64_t{1} << (end % 64U)) - 1U;
            }
            const auto free = ~occupied[word] & mask;
            if (free != 0) {
                return word * 64U + static_cast<std::uint32_t>(std::countr_zero(free));
            }
            begin = (word + 1U) * 64U;
        }
        return std::nullopt;
    }
};
} // namespace detail

SidLease::SidLease(std::shared_ptr<detail::SidPoolState> state, Sid sid) noexcept
    : state_(std::move(state)), sid_(sid) {}
SidLease::~SidLease() { reset(); }
SidLease::SidLease(SidLease&& other) noexcept
    : state_(std::move(other.state_)), sid_(std::exchange(other.sid_, std::nullopt)) {}
SidLease& SidLease::operator=(SidLease&& other) noexcept {
    if (this != &other) {
        reset();
        state_ = std::move(other.state_);
        sid_ = std::exchange(other.sid_, std::nullopt);
    }
    return *this;
}
void SidLease::reset() noexcept {
    if (!sid_) { return; }
    const auto slot = sid_->value() - 1U;
    const auto bit = std::uint64_t{1} << (slot % 64U);
    assert(state_ && state_->active > 0 && (state_->occupied[slot / 64U] & bit) != 0);
    state_->occupied[slot / 64U] &= ~bit;
    --state_->active;
    sid_.reset();
    state_.reset();
}

SidPool::SidPool(std::uint32_t capacity) {
    if (capacity == 0 || capacity > max_sid_value) { throw std::invalid_argument("Invalid SID pool capacity"); }
    state_ = std::make_shared<detail::SidPoolState>(capacity);
}
SidLease SidPool::acquire() noexcept {
    if (state_->active == state_->capacity) { return {}; }
    auto slot = state_->free_slot(state_->next, state_->capacity);
    if (!slot) { slot = state_->free_slot(0, state_->next); }
    assert(slot); // An inactive slot exists whenever active < capacity.
    state_->occupied[*slot / 64U] |= std::uint64_t{1} << (*slot % 64U);
    ++state_->active;
    state_->next = *slot + 1U == state_->capacity ? 0 : *slot + 1U;
    return SidLease(state_, *Sid::from_value(*slot + 1U));
}
bool SidPool::in_use(Sid sid) const noexcept {
    if (sid.value() == 0 || sid.value() > state_->capacity) { return false; }
    const auto slot = sid.value() - 1U;
    return (state_->occupied[slot / 64U] & (std::uint64_t{1} << (slot % 64U))) != 0;
}
std::uint32_t SidPool::capacity() const noexcept { return state_->capacity; }
std::uint32_t SidPool::size() const noexcept { return state_->active; }

} // namespace dc24h::adc
