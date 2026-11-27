#include "dc24h/adc/message_routing.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace { bool deny_allocations = false; }
[[gnu::noinline]] void* operator new(std::size_t size) {
    if (deny_allocations) { throw std::bad_alloc(); }
    if (auto* value = std::malloc(size == 0 ? 1 : size)) { return value; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete(void* value, std::size_t) noexcept { std::free(value); }
[[gnu::noinline]] void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {
using namespace dc24h;
using namespace dc24h::adc;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
Message parsed(std::string_view wire) { return std::get<Message>(parse_msg(wire)); }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
void add(users::SessionManager& sessions, std::uint64_t value, std::string_view sid) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(value), users::Transport::adc, {})), "Add failed");
    require(!sessions.start_negotiation(id(value), {}) &&
        !sessions.identify(id(value), *users::User::create(*users::Identity::create(sid, "CID"+std::to_string(value), "Nick"+std::to_string(value))), {}) &&
        !sessions.mark_ready(id(value), {}), "Ready failed");
}

void decoding_and_errors() {
    for (const auto me : {std::optional<bool>{}, std::optional<bool>{false}, std::optional<bool>{true}}) {
        for (const auto pm : {std::optional<Sid>{}, Sid::parse("AAAB"), Sid::parse("7777")}) {
            const MsgFlags expected{pm, me};
            auto parameters = make_msg_parameters("PMBBBB ME1 Żółw\n\\path", expected);
            const auto fields = std::span<const std::string>(parameters).subspan(1);
            deny_allocations = true;
            const auto result = parse_msg_flags(fields);
            deny_allocations = false;
            require(std::get<MsgFlags>(result) == expected, "Absent/false/true or PM value lost");
            require(parameters[0] == "PMBBBB ME1 Żółw\n\\path", "Positional text was interpreted as flags");
        }
    }
    for (const char* spelling : {"0", "-0", "000", "-000", "1", "01", "0001"}) {
        auto message = parsed(std::string("EMSG AAAB AAAC FlagSecret XXone ME")+spelling+" XXtwo PMAAAB XX");
        const auto original = std::get<std::string>(serialize_msg(message));
        const auto flags = std::get<MsgView>(validate_msg(message)).flags();
        require(flags.private_group == Sid::parse("AAAB") && flags.third_person == (std::string_view(spelling).back() == '1'), "Integer spelling rejected");
        require(original.find(std::string(" ME")+spelling+" ") != std::string::npos, "Codec silently canonicalized input");
        message.parameters.clear();
        require(flags.private_group == Sid::parse("AAAB"), "Flags retained borrowed storage");
    }
    struct Invalid { std::vector<std::string> fields; MsgError error; };
    for (const auto& item : std::array{
        Invalid{{"ME0", "ME0"}, MsgError::duplicate_field},
        Invalid{{"ME0", "XX", "ME1"}, MsgError::duplicate_field},
        Invalid{{"PMAAAB", "PMAAAB"}, MsgError::duplicate_field},
        Invalid{{"PMAAAB", "XX", "PMAAAC"}, MsgError::duplicate_field},
        Invalid{{"PM"}, MsgError::invalid_private_group},
        Invalid{{"PMabc0"}, MsgError::invalid_private_group},
        Invalid{{"PMABC"}, MsgError::invalid_private_group},
        Invalid{{"PMAAABx"}, MsgError::invalid_private_group},
        Invalid{{"ME"}, MsgError::invalid_third_person},
        Invalid{{"ME2"}, MsgError::invalid_third_person},
        Invalid{{"ME-1"}, MsgError::invalid_third_person},
        Invalid{{"ME+1"}, MsgError::invalid_third_person},
        Invalid{{"ME9223372036854775808"}, MsgError::invalid_third_person},
        Invalid{{"ME1\n"}, MsgError::invalid_third_person},
        Invalid{{"bad"}, MsgError::invalid_message}}) {
        deny_allocations = true;
        const auto result = parse_msg_flags(item.fields);
        deny_allocations = false;
        require(std::get<MsgError>(result) == item.error, "Flag error classification differs");
    }
    std::vector<std::string> fields(max_parameters-1, "XX");
    require(std::holds_alternative<MsgFlags>(parse_msg_flags(fields)), "Exact flag count rejected");
    fields.emplace_back("XX");
    require(std::get<MsgError>(parse_msg_flags(fields)) == MsgError::invalid_message, "Excess fields accepted");
    fields = {"XX"+std::string(max_frame_bytes, 'x')};
    require(std::get<MsgError>(parse_msg_flags(fields)) == MsgError::frame_too_long, "Unbounded token accepted");
    for (const auto text : {"", "bad\rtext", "e\xcc\x81"}) {
        bool rejected = false;
        try { (void)make_msg_parameters(text, {}); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid local text accepted");
    }
    auto forged = parsed("EMSG AAAB AAAC FlagSecret PMAAAB ME0");
    forged.parameters.emplace_back(std::string("XX") + char(0xff));
    require(std::holds_alternative<MsgError>(validate_msg(forged)), "Unknown flag bypassed full UTF-8 validation");
    forged.parameters.back() = "XXe\xcc\x81";
    require(std::holds_alternative<MsgError>(validate_msg(forged)), "Unknown flag bypassed full NFC validation");
}

void routing_matrix() {
    users::SessionManager sessions(3); add(sessions, 1, "AAAB"); add(sessions, 2, "AAAC"); add(sessions, 3, "AAAD");
    chat::Core core(sessions); MessageRouter router(core);
    for (const auto mode : {MessageType::broadcast, MessageType::direct, MessageType::echo}) {
        for (const auto me : {std::optional<bool>{}, std::optional<bool>{false}, std::optional<bool>{true}}) {
            for (const bool pm : {false, true}) {
                MsgOptions options; options.type = mode; options.source = Sid::parse("AAAB");
                if (mode != MessageType::broadcast) { options.target = Sid::parse("AAAC"); }
                options.private_group = pm ? options.source : std::nullopt; options.third_person = me;
                auto message = make_msg("FlagSecret\nŻółw", options);
                // An unknown flag's value is opaque, even if it contains PM/ME.
                message.parameters.insert(message.parameters.begin()+1, "XX PMAAAD ME1");
                const auto result = router.prepare(id(1), message);
                if (mode == MessageType::broadcast && pm) {
                    require(std::get<RoutingError>(result) == RoutingError::private_broadcast, "PM became public content");
                    continue;
                }
                const auto& plan = std::get<RoutedMessage>(result);
                std::vector<std::uint64_t> recipients;
                const auto report = router.dispatch(plan, [&](users::SessionId recipient, const RoutedFrame& frame) {
                    recipients.push_back(recipient.value());
                    std::visit([&](const auto& packet) {
                        const auto routed = parsed(packet->wire().substr(0, packet->wire().size()-1));
                        const auto view = std::get<MsgView>(validate_msg(routed));
                        require(view.flags() == MsgFlags{options.private_group, me} && view.text == "FlagSecret\nŻółw", "Projection changed flags/text");
                        require(routed.parameters == make_msg_parameters(view.text, view.flags()), "Projection copied unknown fields");
                    }, frame);
                    return true;
                });
                const auto expected = mode == MessageType::broadcast ? std::vector<std::uint64_t>{1,2,3} :
                    mode == MessageType::echo ? std::vector<std::uint64_t>{2,1} : std::vector<std::uint64_t>{2};
                require(recipients == expected && std::get<chat::DispatchReport>(report).queued == expected.size(), "PM/ME changed recipients");
            }
        }
    }
    auto grouped = parsed("EMSG AAAB AAAC FlagSecret ME1 PMAAAD");
    require(std::get<RoutingError>(router.prepare(id(1), grouped)) == RoutingError::unsupported_group, "Group PM impersonation accepted");
}

void complete_wire_bounds() {
    for (const auto me : {std::optional<bool>{}, std::optional<bool>{false}, std::optional<bool>{true}}) {
        MsgOptions options; options.type = MessageType::echo; options.source = Sid::parse("AAAB");
        options.target = Sid::parse("AAAC"); options.private_group = options.source; options.third_person = me;
        const auto minimal = std::get<std::string>(serialize_msg(make_msg("x", options)));
        const auto size = max_frame_bytes - (minimal.size()-2); // One text byte and LF excluded.
        auto exact = make_msg(std::string(size, 'x'), options);
        require(std::get<std::string>(serialize_msg(exact)).size() == max_frame_bytes+1, "PM/ME wire overhead miscounted");
        exact.parameters[0] += 'x';
        require(std::get<MsgError>(serialize_msg(exact)) == MsgError::frame_too_long, "Flags bypassed complete wire limit");
        // A decoded space costs two encoded bytes; no flag can truncate content.
        exact.parameters[0] = std::string(size/2, ' ')+std::string(size%2, 'x');
        require(std::get<std::string>(serialize_msg(exact)).size() == max_frame_bytes+1, "Escaped flag frame boundary differs");
    }
}
} // namespace
int main() {
    try { decoding_and_errors(); routing_matrix(); complete_wire_bounds(); std::cout << "PASS: PM/ME values, errors, projection, routing and wire bounds\n"; }
    catch (const std::exception& error) { deny_allocations = false; std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
