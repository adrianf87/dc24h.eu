#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace dc24h::chat {
// Fixed outcomes only. No client-controlled strings or identifying metadata.
enum class Event { public_dispatch, private_dispatch, rejected, flood_limited, internal_error };
inline constexpr std::array<std::string_view, 5> event_names{
    "public_dispatch", "private_dispatch", "rejected", "flood_limited", "internal_error"};
inline constexpr std::array<std::string_view, 9> statistic_names{
    "received", "public_dispatches", "private_dispatches", "rejected", "flood_limited", "errors",
    "queued", "unavailable", "refused"};
struct Statistics {
    std::array<std::uint64_t, statistic_names.size()> values{};
    friend bool operator==(const Statistics&, const Statistics&) = default;
};
inline void add_saturating(std::uint64_t& value, std::uint64_t delta = 1) noexcept {
    const auto remaining = std::numeric_limits<std::uint64_t>::max() - value;
    value += delta > remaining ? remaining : delta;
}
// One reactor-thread attempt per decoded client B/D/E MSG, after closing checks.
// Every return path commits one outcome, including validation/admission failure.
// Snapshot reads and accounting allocate nothing and never perform logger I/O.
class Attempt final {
public:
    explicit Attempt(Statistics* statistics) noexcept : statistics_(statistics) {}
    ~Attempt() noexcept {
        if (!statistics_) { return; }
        add_saturating(statistics_->values[0]);
        add_saturating(statistics_->values[1 + static_cast<std::size_t>(event_)]);
        for (std::size_t i = 0; i < delivery_.size(); ++i) { add_saturating(statistics_->values[6+i], delivery_[i]); }
    }
    Attempt(const Attempt&) = delete;
    Attempt& operator=(const Attempt&) = delete;
    void outcome(Event event) noexcept { event_ = event; }
    void dispatched(bool public_message, std::uint64_t queued, std::uint64_t unavailable, std::uint64_t refused) noexcept {
        event_ = public_message ? Event::public_dispatch : Event::private_dispatch;
        delivery_ = {queued, unavailable, refused};
    }
private:
    Statistics* statistics_;
    Event event_{Event::rejected};
    std::array<std::uint64_t, 3> delivery_{};
};
} // namespace dc24h::chat
