#pragma once

#include "dc24h/adc/protocol.hpp"
#include "dc24h/hub/token_bucket.hpp"

#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <variant>
#include <vector>

namespace dc24h::hub {

using WireFrame = std::shared_ptr<const std::string>;

struct Rejection {
    std::string_view code;
    std::string_view description;
    std::string field;

    [[nodiscard]] WireFrame reply(bool fatal = false) const;
};

struct RosterEntry {
    std::string_view sid;
    WireFrame information;
};

struct OnlineUser {
    std::string sid;
    std::string nickname;
    std::string cid;
};

struct ChatDelivery {
    WireFrame frame;
    std::vector<std::string_view> recipients;
    bool directed;
};

struct PeerCapabilities {
    bool tcp;
    bool udp;
    bool adcs;
    bool legacy_adcs;
};

// Call on the server control strand. Returned SID views remain valid until that user is removed.
class Presence {
  public:
    using Clock = std::chrono::steady_clock;
    using ProfileResult = std::variant<WireFrame, Rejection>;
    using ChatResult = std::variant<ChatDelivery, Rejection>;

    Presence(std::size_t nickname_limit, std::size_t message_limit, std::size_t profile_limit);

    // E1 validates the identity first. The transport supplies canonical peer addresses.
    [[nodiscard]] ProfileResult admit(std::string_view sid, const adc::Message &message,
                                      Clock::time_point now = Clock::now());
    [[nodiscard]] ProfileResult update(std::string_view sid, const adc::Message &message,
                                       Clock::time_point now = Clock::now());
    [[nodiscard]] ChatResult chat(std::string_view sid, const adc::Message &message,
                                  Clock::time_point now = Clock::now());

    [[nodiscard]] WireFrame find(std::string_view sid) const;
    [[nodiscard]] std::optional<PeerCapabilities> capabilities(std::string_view sid) const;
    [[nodiscard]] std::optional<std::vector<std::string_view>>
    recipients(const adc::Message &message) const;
    [[nodiscard]] std::vector<RosterEntry> roster() const;
    [[nodiscard]] std::vector<OnlineUser> user_page(std::string_view after,
                                                    std::size_t limit) const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool remove(std::string_view sid);

  private:
    using Fields = std::map<std::string, std::string, std::less<>>;

    struct Profile {
        Fields fields;
        std::string nickname_key;
        std::set<std::string, std::less<>> features;
        WireFrame information;
    };

    struct User {
        Profile profile;
        TokenBucket commands;
    };

    [[nodiscard]] std::optional<Rejection> apply(Profile &profile, const adc::Message &message,
                                                 std::string_view sid, bool initial) const;
    std::size_t nickname_limit_;
    std::size_t message_limit_;
    std::size_t profile_limit_;
    std::map<std::string, User, std::less<>> users_;
};

} // namespace dc24h::hub
