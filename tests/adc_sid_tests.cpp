#include "dc24h/adc/sid_session.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace dc24h::adc;
constexpr auto in = Direction::from_client;
constexpr auto out = Direction::to_client;
void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}
Message parse(std::string_view text) {
    auto result = parse_line(text);
    require(std::holds_alternative<Message>(result), "Invalid SID test fixture");
    return std::get<Message>(std::move(result));
}
void accept(SidSession& session, Direction direction, std::string_view text) {
    require(!session.observe(direction, parse(text)), "Expected accepted SID session event");
}
void negotiated(SidSession& session) {
    accept(session, in, "HSUP ADBASE ADTIGR");
    require(!session.observe(out, *session.support().initial_reply()), "SUP send completion");
}
void identified(SidSession& session) {
    negotiated(session);
    require(!session.prepare_sid(), "SID reservation");
    require(!session.observe(out, *session.sid_reply()), "SID send completion");
    require(session.state() == ConnectionState::identify, "Must reach IDENTIFY");
}
void normal(SidSession& session) {
    identified(session);
    accept(session, in, "BINF " + std::string(session.sid()) + " NIalice");
    require(!session.approve_identity(IdentityMode::guest), "Fixture identity approval");
}

void codec() {
    for (const auto& [value, expected] : std::vector<std::pair<std::uint32_t, std::string>>{
            {0, "AAAA"}, {1, "AAAB"}, {25, "AAAZ"}, {26, "AAA2"}, {31, "AAA7"},
            {32, "AABA"}, {1024, "ABAA"}, {32768, "BAAA"}, {max_sid_value, "7777"}}) {
        const auto sid = Sid::from_value(value);
        require(sid && sid->str() == expected && Sid::parse(expected) == sid, "Base32 vector mismatch");
    }
    for (const auto* invalid : {"", "AAA", "AAAAA", "aaaa", "AAA0", "AAA1", "AAA8", "AAA9",
                                "A AA", "AAA=", "AAA\n", "ĄAA"}) {
        require(!Sid::parse(invalid), "Invalid SID accepted");
    }
    require(!Sid::parse(std::string_view("AA\0A", 4)), "Embedded NUL accepted");
    require(!Sid::from_value(max_sid_value + 1U) &&
            !Sid::from_value(std::numeric_limits<std::uint32_t>::max()), "Out-of-domain value accepted");
    // Exhaust the actual 20-bit grammar domain, including zero syntactically.
    for (std::uint32_t value = 0; value <= max_sid_value; ++value) {
        const auto sid = *Sid::from_value(value);
        const auto encoded = sid.encode();
        require(Sid::parse(std::string_view(encoded.data(), encoded.size())) == sid, "Full-domain round trip");
    }
}

void ownership_and_lifetime() {
    for (const auto capacity : {0U, max_sid_value + 1U, std::numeric_limits<std::uint32_t>::max()}) {
        bool rejected = false;
        try { SidPool invalid(capacity); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid SID capacity accepted");
    }
    SidPool pool(2);
    auto first = pool.acquire();
    auto second = pool.acquire();
    const auto first_id = *first.sid();
    const auto second_id = *second.sid();
    require(pool.size() == 2 && first_id != second_id && !pool.acquire(), "Unique bounded reservations");
    auto moved = std::move(first);
    first.reset();
    require(!first && !first.sid() && moved.sid() == first_id && pool.size() == 2, "Move ownership");
    second = std::move(moved); // Releases second_id before taking first_id.
    require(!moved && second.sid() == first_id && pool.size() == 1 && !pool.in_use(second_id), "Move assignment");
    auto reused = pool.acquire();
    require(reused.sid() == second_id, "Reuse free slot only");
    moved.reset();
    first.reset();
    require(pool.in_use(second_id) && pool.size() == 2, "Moved-from release freed a reused SID");
    auto& alias = second;
    second = std::move(alias);
    require(second.sid() == first_id && pool.size() == 2, "Self-move released reservation");
    second.reset(); second.reset(); reused.reset();
    require(pool.size() == 0, "Idempotent reset");
    SidLease survivor;
    {
        SidPool temporary(1);
        survivor = temporary.acquire();
    }
    require(survivor.sid()->str() == "AAAB", "Lease did not survive pool owner destruction");
    survivor.reset(); // ASan checks shared state lifetime, not a dangling pool pointer.
    require(!survivor, "Survivor release");
    SidPool other(2);
    auto independent = other.acquire();
    require(other.size() == 1 && pool.size() == 0, "Independent hubs share no allocation state");
    require(!pool.in_use(*Sid::from_value(0)) && !pool.in_use(*Sid::from_value(3)), "Outside client range");
}

void word_boundaries_and_churn() {
    for (const auto capacity : {1U, 2U, 63U, 64U, 65U, 127U, 128U, 129U}) {
        SidPool pool(capacity);
        std::vector<SidLease> leases;
        for (std::uint32_t i = 0; i < capacity; ++i) {
            leases.push_back(pool.acquire());
            require(leases.back().sid()->value() == i + 1U, "Deterministic first pass");
        }
        require(!pool.acquire(), "Exact exhaustion");
        for (std::uint32_t i = 0; i < capacity; i += 2U) { leases[i].reset(); }
        for (std::uint32_t i = 0; i < capacity; i += 2U) {
            auto next = pool.acquire();
            require(next.sid()->value() == i + 1U, "Wrapped holes/word mask");
            leases[i] = std::move(next);
        }
        require(pool.size() == capacity && !pool.acquire(), "Refill exhaustion");
        std::uint32_t random = 0x5a17U;
        for (unsigned step = 0; step < 1000; ++step) {
            random = random * 1664525U + 1013904223U;
            const auto slot = random % capacity;
            const auto old = leases[slot].sid();
            leases[slot].reset();
            leases[slot] = pool.acquire();
            require(leases[slot].sid() == old && pool.size() == capacity, "Deterministic churn collision");
        }
        leases.clear();
        require(pool.size() == 0, "Scope cleanup across word boundaries");
    }
}

void full_namespace() {
    SidPool pool;
    std::vector<SidLease> leases;
    leases.reserve(max_sid_value);
    for (std::uint32_t i = 1; i <= max_sid_value; ++i) {
        leases.push_back(pool.acquire());
        require(leases.back().sid()->value() == i, "Full namespace uniqueness/range");
    }
    require(pool.size() == max_sid_value && !pool.acquire(), "Full namespace exhaustion");
    require(leases.back().sid()->str() == "7777", "Upper endpoint unused");
    leases.back().reset();
    auto tail = pool.acquire();
    require(tail.sid()->value() == max_sid_value, "Scan final partially filled bitmap word");
    leases.front().reset();
    auto head = pool.acquire();
    require(head.sid()->value() == 1, "Wrap back to first SID");
    leases.clear(); head.reset(); tail.reset();
    require(pool.size() == 0, "Full namespace cleanup");
}

void session_order_and_errors() {
    bool invalid = false;
    try { SidSession missing(nullptr); } catch (const std::invalid_argument&) { invalid = true; }
    require(invalid, "Null pool accepted");
    auto pool = std::make_shared<SidPool>(1);
    SidSession session(pool);
    require(!session.sid_reply() && !session.sid_reply_line() && !session.reserved_sid(), "No early SID reply");
    require(session.prepare_sid() == SidError::invalid_sequence, "No SID before SUP");
    accept(session, in, "HSUP ADBASE ADTIGR");
    require(session.prepare_sid() == SidError::invalid_sequence && pool->size() == 0, "No SID before SUP send");
    require(session.observe(out, parse("ISID AAAB")) == SidSessionError{SidError::not_reserved}, "No external SID");
    require(!session.observe(out, *session.support().initial_reply()), "SUP send");
    require(!session.prepare_sid() && pool->size() == 1 && session.sid().empty(), "Reserve without advance");
    require(session.prepare_sid() == SidError::invalid_sequence && pool->size() == 1, "No duplicate reservation");
    require(session.sid_reply_line() == "ISID AAAB\n", "Exact owned reply encoding");
    for (const char* wrong : {"ISID AAAA", "ISID AAAC", "ISID AAAB XXextra", "HSID AAAB"}) {
        require(session.observe(out, parse(wrong)) == SidSessionError{SidError::reply_mismatch}, "Wrong SID reply accepted");
        require(session.state() == ConnectionState::protocol && pool->size() == 1, "Wrong reply changed reservation/state");
    }
    auto reply = *session.sid_reply();
    reply.parameters.clear();
    require(session.observe(out, reply) == SidSessionError{SidError::reply_mismatch}, "Missing SID guard");
    require(!session.observe(out, *session.sid_reply()), "Completed assigned SID");
    require(session.state() == ConnectionState::identify && session.sid() == "AAAB", "Assigned SID binding");
    require(session.prepare_sid() == SidError::invalid_state, "No second identity allocation");
    require(session.observe(out, parse("ISID AAAB")) == SidSessionError{SidError::invalid_state}, "No SID replay");
    require(session.observe(in, parse("BINF AAAC NIspoof")) == SidSessionError{StateError::wrong_source}, "Wrong source");
    accept(session, in, "BINF AAAB NIalice");
    require(!session.approve_identity(IdentityMode::password_required), "Registered path");
    accept(session, out, "IGPA challenge");
    accept(session, in, "HPAS response");
    require(!session.approve_password() && pool->size() == 1, "VERIFY retains reservation");
    require(session.observe(in, parse("HSUP RMTIGR")) == SidSessionError{SupportError::session_hash_removed}, "SUP gate lost");
    accept(session, out, "IQUI AAAC");
    require(pool->size() == 1, "Other-user QUI released this SID");
    accept(session, out, "IQUI AAAB");
    require(session.state() == ConnectionState::disconnected && pool->size() == 0 && !session.reserved_sid(), "Own QUI cleanup");
    session.disconnect();
    require(session.prepare_sid() == SidError::invalid_state && !session.sid_reply(), "No closed-session reuse");
    require(session.observe(in, parse("HSUP ADBASE ADTIGR")) == SidSessionError{StateError::disconnected}, "Terminal gate");
    require(sid_status_code(SidError::exhausted) == "211" && sid_status_code(SidError::reply_mismatch) == "240",
            "Fixed SID status codes");
}

void shared_sessions_and_cleanup() {
    auto pool = std::make_shared<SidPool>(2);
    SidSession first(pool), second(pool), waiting(pool);
    identified(first); identified(second); negotiated(waiting);
    require(first.sid() != second.sid() && pool->size() == 2, "Concurrent live session collision");
    const std::string released(first.sid());
    require(waiting.prepare_sid() == SidError::exhausted && waiting.state() == ConnectionState::protocol &&
            !waiting.reserved_sid() && pool->size() == 2, "Exhaustion mutation");
    first.disconnect();
    require(!waiting.prepare_sid() && waiting.reserved_sid()->str() == released, "Reuse after disconnect");
    first.disconnect();
    require(pool->size() == 2, "Stale session released a new owner's reservation");
    require(!waiting.observe(out, *waiting.sid_reply()), "Reused binding");
    second.disconnect(); waiting.disconnect();
    require(pool->size() == 0, "Shared cleanup");
    for (unsigned phase = 0; phase < 4; ++phase) {
        SidSession session(pool);
        if (phase == 0) { negotiated(session); require(!session.prepare_sid(), "Pending send fixture"); }
        else if (phase == 1) { identified(session); }
        else if (phase == 2) {
            identified(session);
            accept(session, in, "BINF " + std::string(session.sid()) + " NIregistered");
            require(!session.approve_identity(IdentityMode::password_required), "VERIFY fixture");
        } else { normal(session); }
        require(pool->size() == 1, "Live lease missing");
        accept(session, phase % 2 == 0 ? in : out, phase % 2 == 0 ? "HSTA 200 rejected" : "ISTA 200 rejected");
        require(pool->size() == 0 && session.state() == ConnectionState::disconnected, "Fatal status cleanup");
    }
    {
        SidSession startup_failure(pool);
        negotiated(startup_failure);
        require(!startup_failure.prepare_sid(), "Failed send fixture");
    }
    require(pool->size() == 0, "Destruction leaked pending SID");
    auto lifetime = std::make_unique<SidSession>(pool);
    pool.reset();
    identified(*lifetime); // Session owns allocator lifetime.
    lifetime.reset();
}

void ncdc_transcript() {
    auto pool = std::make_shared<SidPool>(4);
    SidSession session(pool);
    std::string text;
    require(static_cast<bool>(std::getline(std::cin, text)), "Missing ncdc SUP");
    accept(session, in, text);
    std::cout << *session.support().initial_reply_line() << std::flush;
    require(static_cast<bool>(std::getline(std::cin, text)) && text == "SUP_SENT", "Missing SUP completion");
    require(!session.observe(out, *session.support().initial_reply()), "Committed SUP");
    require(!session.prepare_sid(), "Real ncdc SID reservation");
    std::cout << *session.sid_reply_line() << std::flush;
    require(static_cast<bool>(std::getline(std::cin, text)) && text == "SID_SENT", "Missing SID completion");
    require(!session.observe(out, *session.sid_reply()), "Committed SID");
    require(static_cast<bool>(std::getline(std::cin, text)), "Missing ncdc INF");
    const auto inf = parse(text);
    require(inf.kind == Command::inf && inf.type == MessageType::broadcast, "Expected ncdc BINF");
    require(!session.observe(in, inf) && session.state() == ConnectionState::identify && pool->size() == 1,
            "ncdc did not use assigned SID");
    session.disconnect();
    require(pool->size() == 0 && !session.reserved_sid(), "ncdc fixture SID cleanup");
    std::cout << "IDENTIFY SID_RELEASED\n" << std::flush;
}
} // namespace

int main(int argc, char** argv) {
    static_assert(!std::is_copy_constructible_v<SidLease> && std::is_nothrow_move_constructible_v<SidLease>);
    static_assert(!std::is_copy_constructible_v<SidPool> && !std::is_move_constructible_v<SidPool>);
    static_assert(!std::is_copy_constructible_v<SidSession> && !std::is_move_constructible_v<SidSession>);
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--ncdc-transcript") { ncdc_transcript(); return 0; }
        require(argc == 1, "Unexpected SID test arguments");
        codec(); ownership_and_lifetime(); word_boundaries_and_churn(); full_namespace();
        session_order_and_errors(); shared_sessions_and_cleanup();
        std::cout << "PASS: 20-bit SID codec, complete pool, RAII, churn and managed sessions\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
