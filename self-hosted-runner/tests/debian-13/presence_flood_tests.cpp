#include "dc24h/adc/presence_flood.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace dc24h;
using namespace std::chrono_literals;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
void buckets() {
    const auto start = adc::PresenceClock::time_point{};
    adc::PresenceGuard guard;
    for (std::uint32_t index = 0; index < adc::presence_update_burst; ++index) {
        require(guard.admit(start).allowed, "Initial burst missing");
    }
    require(guard.admit(start) == adc::PresenceDecision{false, true}, "First denial did not notify");
    require(guard.admit(start) == adc::PresenceDecision{false, false}, "Repeated denial was not silent");
    require(!guard.admit(start + 250ms - 1ns).allowed, "Fractional refill minted a token early");
    require(guard.admit(start + 250ms).allowed, "One second of rate did not return one update");
    const auto still = guard.admit(start + 250ms);
    require(still == adc::PresenceDecision{false, false}, "Success reset the notice window");
    adc::PresenceGuard recovered;
    for (std::uint32_t index = 0; index < adc::presence_update_burst; ++index) { require(recovered.admit(start).allowed, "Second burst missing"); }
    require(recovered.admit(start) == adc::PresenceDecision{false, true}, "Second guard did not empty");
    for (std::uint32_t index = 0; index < adc::presence_updates_per_second; ++index) {
        require(recovered.admit(start + 1s).allowed, "One second did not refill the configured rate");
    }
    const auto notice = recovered.admit(start + 1s);
    require(notice == adc::PresenceDecision{false, true}, "Notice did not return after one second");
    require(recovered.admit(start) == adc::PresenceDecision{false, false}, "Backwards clock minted credit or a notice");
    adc::PresenceGuard extremes;
    require(extremes.admit(adc::PresenceClock::time_point::min()).allowed, "Minimum clock rejected a full bucket");
    for (std::uint32_t index = 1; index < adc::presence_update_burst; ++index) {
        require(extremes.admit(adc::PresenceClock::time_point::min()).allowed, "Minimum clock burst truncated");
    }
    require(!extremes.admit(adc::PresenceClock::time_point::min()).allowed, "Minimum clock did not exhaust");
    require(extremes.admit(adc::PresenceClock::time_point::max()).allowed, "Maximum clock did not saturate the bucket");
}
adc::Message parse(std::string_view value) { return std::get<adc::Message>(adc::parse_line(value)); }
std::string drain(adc::LoginHub& hub, std::uint64_t owner) {
    std::string result;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) {
        const std::string wire(hub.output(owner));
        result += wire;
        hub.consume(owner, wire.size());
    }
    return result;
}
std::string join(adc::LoginHub& hub, std::uint64_t owner) {
    hub.connect(owner, "127.0.0.1");
    hub.receive(owner, parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner, hub.output(owner).size());
    const std::string sid(hub.output(owner).substr(5, 4));
    hub.consume(owner, hub.output(owner).size());
    adc::TigerDigest pid{};
    pid[0] = static_cast<std::uint8_t>(owner);
    hub.receive(owner, parse("BINF " + sid + " NIUser" + std::to_string(owner) + " ID" + adc::tiger_base32(*adc::tiger192(pid)) + " PD" + adc::tiger_base32(pid)));
    (void)drain(hub, owner);
    return sid;
}
void live_adapter() {
    adc::LoginLimits limits;
    limits.anti_flood = chat::FloodLimits{};
    adc::LoginHub hub(limits);
    const auto owner = join(hub, 1);
    const auto observer = join(hub, 2);
    (void)drain(hub, 1);
    const auto now = adc::PresenceClock::time_point{};
    for (int index = 0; index < 8; ++index) {
        const auto frame = "BINF " + owner + " SS" + std::to_string(index);
        hub.receive(1, parse(frame), now);
        require(drain(hub, 1) == frame + "\n" && drain(hub, 2) == frame + "\n", "Public presence delta lost");
    }
    hub.receive(1, parse("BINF " + owner + " SS8"), now);
    require(drain(hub, 1) == "ISTA 100 Presence\\supdate\\srate\\slimit\\sexceeded FCBINF\n" && hub.output(2).empty() && hub.ready(1),
        "Denied presence update was applied, relayed or closed the session");
    hub.receive(1, parse("BINF " + owner + " SS9"), now);
    require(hub.output(1).empty() && hub.output(2).empty(), "Repeated presence denial amplified output");
    hub.receive(1, parse("BMSG " + owner + " ValidAfterSyntax"), now);
    require(drain(hub, 1) == "BMSG " + owner + " ValidAfterSyntax\n" && drain(hub, 2) == "BMSG " + owner + " ValidAfterSyntax\n",
        "Chat budget was tied to the presence bucket");
    hub.receive(1, parse("BINF " + owner + " SS10"), now + 1s);
    require(drain(hub, 1) == "BINF " + owner + " SS10\n" && drain(hub, 2) == "BINF " + owner + " SS10\n", "Refill could not resume presence");
}
}
int main() {
    try {
        buckets();
        live_adapter();
        std::cout << "PASS: presence burst, one notice per second, no disconnect, independent chat\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
