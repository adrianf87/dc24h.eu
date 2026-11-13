#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace dc24h::search {
inline constexpr std::array<std::string_view,19> statistic_names{
    "queries_received","queries_routed","queries_rejected","queries_rate_limited","queries_ignored","query_errors",
    "query_queued","query_unavailable","query_refused","query_canceled",
    "results_received","results_routed","results_rejected","results_rate_limited","results_recipient_limited","result_errors",
    "result_queued","result_unavailable","result_refused"};
struct Statistics {
    std::array<std::uint64_t,statistic_names.size()> values{};
    // Stored token windows; expiry is lazy. Not a count of UDP exchanges/files.
    std::uint64_t tracked_searches{};
    friend bool operator==(const Statistics&,const Statistics&) = default;
};
enum class Kind { query, result };
enum class Outcome { routed, rejected, rate_limited, ignored, recipient_limited, error };
inline void add_saturating(std::uint64_t& value,std::uint64_t delta=1) noexcept {
    const auto room=std::numeric_limits<std::uint64_t>::max()-value;
    value+=delta>room?room:delta;
}
// One owner-thread decoded SCH/RES attempt after closing checks. Exactly one
// terminal outcome, including silent denials and early source/state rejection.
// Query ignores and recipient-limited results are distinct outcomes.
class Attempt final {
public:
    Attempt(Statistics* statistics,Kind kind) noexcept : statistics_(statistics),kind_(kind) {}
    ~Attempt() noexcept {
        if(!statistics_){return;}
        const std::size_t base=kind_==Kind::query?0:10;
        add_saturating(statistics_->values[base]);
        std::size_t outcome=base+2;
        switch(outcome_){
        case Outcome::routed: outcome=base+1;break;
        case Outcome::rejected: break;
        case Outcome::rate_limited: outcome=base+3;break;
        case Outcome::ignored: if(kind_==Kind::query){outcome=4;}break;
        case Outcome::recipient_limited: if(kind_==Kind::result){outcome=14;}break;
        case Outcome::error: outcome=base+5;break;
        }
        add_saturating(statistics_->values[outcome]);
        const auto deliveries=kind_==Kind::query?4U:3U;
        for(std::size_t i=0;i<deliveries;++i){add_saturating(statistics_->values[base+6+i],delivery_[i]);}
    }
    Attempt(const Attempt&)=delete;
    Attempt& operator=(const Attempt&)=delete;
    void outcome(Outcome value) noexcept {outcome_=value;}
    void delivery(std::uint64_t queued,std::uint64_t unavailable,std::uint64_t refused,std::uint64_t canceled=0) noexcept {
        delivery_={queued,unavailable,refused,canceled};
    }
private:
    Statistics* statistics_;
    Kind kind_;
    Outcome outcome_{Outcome::rejected};
    std::array<std::uint64_t,4> delivery_{};
};
} // namespace dc24h::search
