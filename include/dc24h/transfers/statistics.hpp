#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace dc24h::transfers {
inline constexpr std::array<std::string_view,22> statistic_names{
    "ctm_received", "ctm_routed", "ctm_rejected", "ctm_rate_limited", "ctm_recipient_limited", "ctm_errors",
    "ctm_queued", "ctm_refused", "ctm_canceled", "ctm_queue_errors",
    "rcm_received", "rcm_routed", "rcm_rejected", "rcm_rate_limited", "rcm_recipient_limited", "rcm_errors",
    "rcm_queued", "rcm_refused", "rcm_canceled", "rcm_queue_errors",
    "peer_protocol_unsupported", "peer_connection_failed"};
struct Statistics {
    std::array<std::uint64_t,statistic_names.size()> values{};
    friend bool operator==(const Statistics&,const Statistics&) = default;
};
inline void add_saturating(std::uint64_t& value,std::uint64_t delta=1) noexcept {
    const auto remaining=std::numeric_limits<std::uint64_t>::max()-value;
    value+=delta>remaining?remaining:delta;
}
enum class Operation { ctm, rcm };
enum class Outcome { routed, rejected, rate_limited, recipient_limited, error };
// One decoded client CTM/RCM after the closing gate, including early rejection
// and allocation failure. One outcome per request; delivery counters count queue
// copies (E may queue twice), not established peer connections or file bytes.
// Owner thread only; all operations/snapshots are fixed-size and allocation-free.
class Attempt final {
public:
    Attempt(Statistics* statistics,Operation operation) noexcept : statistics_(statistics),operation_(operation) {}
    ~Attempt() noexcept {
        if(!statistics_){return;}
        const std::size_t base=operation_==Operation::ctm?0:10;
        add_saturating(statistics_->values[base]);
        add_saturating(statistics_->values[base+1+static_cast<std::size_t>(outcome_)]);
        for(std::size_t i=0;i<delivery_.size();++i){add_saturating(statistics_->values[base+6+i],delivery_[i]);}
    }
    Attempt(const Attempt&)=delete;
    Attempt& operator=(const Attempt&)=delete;
    void outcome(Outcome value) noexcept {outcome_=value;}
    void delivery(std::uint64_t queued,std::uint64_t refused,std::uint64_t canceled) noexcept {
        delivery_[0]=queued;delivery_[1]=refused;delivery_[2]=canceled;
    }
    void queue_error() noexcept {add_saturating(delivery_[3]);}
private:
    Statistics* statistics_;
    Operation operation_;
    Outcome outcome_{Outcome::rejected};
    std::array<std::uint64_t,4> delivery_{};
};
// Count each accepted D/E peer error report once, independent of E echo and
// severity. These are client claims without pending-token/connection correlation.
inline void peer_report(Statistics& statistics,unsigned error) noexcept {
    if(error==41){add_saturating(statistics.values[20]);}
    else if(error==42){add_saturating(statistics.values[21]);}
}
} // namespace dc24h::transfers
