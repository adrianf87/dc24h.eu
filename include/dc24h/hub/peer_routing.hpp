#pragma once

#include "dc24h/hub/presence.hpp"

#include <compare>
#include <functional>
#include <utility>

namespace dc24h::hub {

// The server control strand owns the signaling state and invokes the transport callback.
class PeerRouting {
  public:
    using Clock = TokenBucket::Clock;
    using Send = std::function<bool(std::string_view, const WireFrame &)>;

    PeerRouting(const Presence &presence, Send send);
    [[nodiscard]] std::optional<Rejection> route(std::string_view sid, const adc::Message &message,
                                                 Clock::time_point now = Clock::now());
    void forget(std::string_view sid);

  private:
    struct Limits {
        explicit Limits(Clock::time_point now);

        TokenBucket searches;
        TokenBucket results;
        TokenBucket connections;
    };

    using SearchKey = std::pair<std::string, std::string>; // Requester SID and token.

    struct Responder {
        Clock::time_point deadline;
        std::size_t remaining = 0;
    };

    struct ReverseRequest {
        std::string active;
        std::string requester;
        std::string protocol;
        std::string token;

        auto operator<=>(const ReverseRequest &) const = default;
    };

    [[nodiscard]] std::optional<Rejection> search(std::string_view sid, const adc::Message &message,
                                                  PeerCapabilities source, Clock::time_point now);
    [[nodiscard]] std::optional<Rejection> result(std::string_view sid, const adc::Message &message,
                                                  Clock::time_point now);
    [[nodiscard]] std::optional<Rejection> connect(std::string_view sid,
                                                   const adc::Message &message,
                                                   PeerCapabilities source, Clock::time_point now);
    [[nodiscard]] bool send_directed(const adc::Message &message);
    void expire(Clock::time_point now);

    const Presence &presence_;
    Send send_;
    std::map<std::string, Limits, std::less<>> limits_;
    std::map<SearchKey, std::map<std::string, Responder, std::less<>>> searches_;
    std::map<ReverseRequest, Clock::time_point> reverse_;
    std::size_t responder_count_ = 0;
    Clock::time_point next_expiry_{};
};

} // namespace dc24h::hub
