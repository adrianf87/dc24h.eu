#include "dc24h/security/limits.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* explanation) {
    if (!condition) { throw std::runtime_error(explanation); }
}
void check_validation() {
    using dc24h::security::Limits;
    using dc24h::security::validate;
    validate({});
    auto test = [](auto edit) {
        Limits limits;
        edit(limits);
        bool rejected = false;
        try { validate(limits); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid security setting was accepted");
    };
    Limits upper;
    upper.max_connections_per_ip = 65535;
    upper.max_accepts_per_minute = 100000;
    upper.max_frames_per_second = 100000;
    upper.max_invalid_frames = 65535;
    upper.tls_handshake_timeout = std::chrono::seconds(3600);
    upper.partial_frame_timeout = std::chrono::seconds(3600);
    upper.login_timeout = std::chrono::seconds(3600);
    upper.output_timeout = std::chrono::seconds(3600);
    validate(upper);
    test([](Limits& l) { l.max_connections_per_ip = 65536; });
    test([](Limits& l) { l.max_accepts_per_minute = 100001; });
    test([](Limits& l) { l.max_frames_per_second = 100001; });
    test([](Limits& l) { l.max_invalid_frames = 65536; });
    test([](Limits& l) { l.tls_handshake_timeout = std::chrono::seconds(3601); });
    test([](Limits& l) { l.partial_frame_timeout = std::chrono::seconds(3601); });
    test([](Limits& l) { l.max_connections_per_ip = 0; });
    test([](Limits& l) { l.max_accepts_per_minute = 0; });
    test([](Limits& l) { l.max_frames_per_second = 0; });
    test([](Limits& l) { l.max_invalid_frames = 0; });
    test([](Limits& l) { l.tls_handshake_timeout = std::chrono::seconds(0); });
    test([](Limits& l) { l.partial_frame_timeout = std::chrono::seconds(0); });
    test([](Limits& l) { l.login_timeout = std::chrono::seconds(0); });
    test([](Limits& l) { l.output_timeout = std::chrono::seconds(0); });
    test([](Limits& l) { l.login_timeout = std::chrono::seconds(3601); });
    test([](Limits& l) { l.output_timeout = std::chrono::seconds(3601); });
}
void check_peers() {
    using namespace std::chrono_literals;
    dc24h::security::Limits limits;
    limits.max_connections_per_ip = 1;
    limits.max_accepts_per_minute = 2;
    dc24h::security::PeerLimiter limiter(limits);
    const dc24h::security::Clock::time_point start{};
    require(limiter.admit("127.0.0.1", start), "First peer was rejected");
    require(!limiter.admit("127.0.0.1", start), "Per-IP concurrent quota was bypassed");
    require(limiter.admit("::1", start), "Other IP was incorrectly limited");
    limiter.release("127.0.0.1");
    require(limiter.admit("127.0.0.1", start), "Released IP slot cannot be reused");
    limiter.release("127.0.0.1");
    require(!limiter.admit("127.0.0.1", start), "Per-minute quota was bypassed");
    require(!limiter.admit("127.0.0.1", start + 59999ms), "Minute budget reset early");
    require(limiter.admit("127.0.0.1", start + 60s), "Per-minute quota never resets");
    limiter.release("127.0.0.1");
    limiter.release("::1");
    require(limiter.admit("192.0.2.1", start + 70s), "Pruning left broken limiter state");
    require(limiter.tracked_peers() <= 3, "Unexpected peer map growth");
}
void bounded_map_and_pruning() {
    using namespace std::chrono_literals;
    using dc24h::security::Clock;
    dc24h::security::PeerLimiter limiter({});
    const Clock::time_point start{};
    require(!limiter.admit("", start), "empty identity admitted");
    require(!limiter.admit(std::string(dc24h::security::max_peer_key_bytes + 1, 'x'), start),
            "unbounded identity admitted");
    require(limiter.tracked_peers() == 0, "invalid identities consumed state");
    for (std::size_t i = 0; i < dc24h::security::max_tracked_peers; ++i) {
        const auto key = "4:192.0." + std::to_string(i / 256) + "." + std::to_string(i % 256);
        require(limiter.admit(key, start), "map rejected before its cap");
        limiter.release(key);
    }
    require(limiter.tracked_peers() == dc24h::security::max_tracked_peers, "map cap mismatch");
    require(!limiter.admit("4:198.51.100.1", start), "map grew beyond its cap");
    require(limiter.admit("4:198.51.100.1", start + 60s), "expired entries did not free capacity");
    require(limiter.tracked_peers() == 1, "expired inactive entries were retained");
    require(limiter.admit("4:203.0.113.1", start + 120s), "new identity rejected");
    require(limiter.tracked_peers() == 2, "active identity was evicted during pruning");
    limiter.release("missing");
    limiter.release("4:198.51.100.1");
    limiter.release("4:198.51.100.1");
}

void check_frames() {
    using namespace std::chrono_literals;
    const dc24h::security::Clock::time_point start{};
    dc24h::security::FrameLimiter limiter(2);
    require(limiter.admit(start), "First frame rejected");
    require(limiter.admit(start + 100ms), "Second frame rejected");
    require(!limiter.admit(start + 200ms), "Frame flood was not rejected");
    require(!limiter.admit(start + 999ms), "Frame window reset early");
    require(limiter.admit(start + 1s), "Frame window did not reset");
}
} // namespace
int main() {
    try {
        check_validation();
        check_peers();
        check_frames();
        bounded_map_and_pruning();
        std::cout << "Security quotas and validation OK\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
