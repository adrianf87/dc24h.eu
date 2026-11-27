#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include "dc24h/bans/bans.hpp"
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {
using namespace dc24h;
using namespace dc24h::adc;
constexpr std::string_view password = "ADC wire password 42!";
void require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("Account security assertion at line " + std::to_string(where.line())); }
}
Message parse(std::string_view text) { return std::get<Message>(parse_line(text)); }
TigerDigest pid(unsigned seed) {
    TigerDigest value{};
    for (std::size_t i = 0; i < value.size(); ++i) { value[i] = static_cast<unsigned char>(i + seed); }
    return value;
}
std::string cid(unsigned seed) { return tiger_base32(*tiger192(pid(seed))); }
struct Source final : AuthenticationSource {
    mutable unsigned preparations{0};
    mutable unsigned opened{0};
    AuthenticationPreparation prepare(std::string_view nick, std::string_view supplied,
        users::Transport, PasswordChallenge&) const override {
        ++preparations;
        for (unsigned i = 1; i <= 2; ++i) {
            if (nick == "Role" + std::to_string(i) || supplied == cid(i)) {
                if (nick != "Role" + std::to_string(i) || supplied != cid(i)) { return DeniedAuthentication{}; }
                return PasswordAuthentication{i, Message{MessageType::info, {}, Command::unknown, {}, {}, {}, {}, {}}, accounts::Role::registered, true};
            }
        }
        return GuestAuthentication{};
    }
    std::optional<Message> open_challenge(std::uint64_t account, PasswordChallenge& challenge) const override {
        ++opened;
        auto issued = challenge.issue(password, SessionHash::tiger192);
        if (account == 0 || account > 2 || !std::holds_alternative<Message>(issued)) { return std::nullopt; }
        return std::get<Message>(std::move(issued));
    }
    bool allows_identity(std::string_view, std::string_view, std::optional<std::uint64_t>) const noexcept override { return true; }
};
std::string drain(LoginHub& hub, std::uint64_t id) {
    std::string output;
    while (!hub.output(id).empty()) { const auto wire = hub.output(id); output += wire; (void)hub.consume(id, wire.size()); }
    return output;
}
std::string identify(LoginHub& hub, std::uint64_t id, unsigned seed, std::string_view peer, std::string_view nick) {
    hub.connect(id, peer, users::Transport::adcs);
    hub.transport_ready(id);
    hub.receive(id, parse("HSUP ADBASE ADTIGR"));
    const auto initial = drain(hub, id);
    const auto sid = initial.substr(initial.find("ISID ") + 5, 4);
    hub.receive(id, parse("BINF " + sid + " NI" + std::string(nick) + " ID" + cid(seed) + " PD" + tiger_base32(pid(seed)) + " I40.0.0.0"));
    return sid;
}
void expect_closed(LoginHub& hub, std::uint64_t id, std::string_view status) {
    const auto text = drain(hub, id);
    require(text.find("IGPA ") == std::string::npos);
    require(text.find(status) != std::string::npos);
    require(text.find(password) == std::string::npos);
    require(hub.closing(id) && !hub.authenticated_account(id));
}
void admission() {
    auto source = std::make_shared<Source>();
    LoginLimits limits;
    limits.authentication = source;
    limits.authentication_limits.attempts_per_account = 1;
    LoginHub hub(limits);
    (void)identify(hub, 1, 1, "127.0.0.1", "Role1");
    require(drain(hub, 1).starts_with("IGPA ") && source->opened == 1 && source->preparations == 1);
    (void)identify(hub, 2, 1, "127.0.0.2", "Role1");
    expect_closed(hub, 2, "ISTA 223 ");
    require(source->opened == 1 && source->preparations == 2);
    (void)identify(hub, 3, 2, "127.0.0.3", "Role2");
    require(drain(hub, 3).starts_with("IGPA ") && source->opened == 2);
    (void)identify(hub, 4, 9, "127.0.0.4", "Guest9");
    const auto guest = drain(hub, 4);
    require(guest.find("IGPA ") == std::string::npos && guest.find("ISTA 223 ") == std::string::npos && source->opened == 2);
    (void)identify(hub, 5, 2, "127.0.0.5", "Role1");
    expect_closed(hub, 5, "ISTA 223 ");
    require(source->opened == 2);
}
void banned() {
    auto source = std::make_shared<Source>();
    LoginLimits limits;
    limits.authentication = source;
    limits.bans = std::make_shared<bans::Directory>(std::vector<bans::Rule>{{bans::Kind::account, "1", std::nullopt}});
    LoginHub hub(limits);
    (void)identify(hub, 1, 1, "127.0.0.1", "Role1");
    expect_closed(hub, 1, "ISTA 231 ");
    require(source->preparations == 1 && source->opened == 0);
}
}
int main() {
    try { admission(); banned(); std::cout << "Account security checks passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
