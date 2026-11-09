#include "dc24h/adc/inf.hpp"
#include "dc24h/adc/tiger_identity.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using namespace dc24h::adc;
void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}
Message parse(std::string_view text) {
    auto result = parse_line(text);
    require(std::holds_alternative<Message>(result), "Invalid identity fixture syntax");
    return std::get<Message>(std::move(result));
}
std::string cid_for(const TigerDigest& pid) {
    const auto digest = tiger192(pid);
    require(digest.has_value(), "Tiger provider unavailable");
    return tiger_base32(*digest);
}
void expect_error(std::string_view cid, std::string_view pid, IdentityError error) {
    const auto result = verify_tiger_identity(cid, pid);
    require(std::holds_alternative<IdentityError>(result) && std::get<IdentityError>(result) == error,
            "Unexpected Tiger identity diagnostic");
}
void reference_vectors() {
    const std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
    const TigerDigest expected{
        0x2a,0xab,0x14,0x84,0xe8,0xc1,0x58,0xf2,0xbf,0xb8,0xc5,0xff,
        0x41,0xb5,0x7a,0x52,0x51,0x29,0x13,0x1c,0x95,0x7b,0x5f,0x93};
    TigerDigest direct{};
    require(tiger192_into(abc, direct) && direct == expected, "Standard Tiger reference/byte order");
    const auto empty = tiger192({});
    const TigerDigest expected_empty{
        0x32,0x93,0xac,0x63,0x0c,0x13,0xf0,0x24,0x5f,0x92,0xbb,0xb1,
        0x76,0x6e,0x16,0x16,0x7a,0x4e,0x58,0x49,0x2d,0xde,0x73,0xf3};
    require(empty && *empty == expected_empty && tiger192_into({}, direct) && direct == expected_empty,
            "Empty Tiger reference vector");
    // The existing INF suite also checks the published empty/abc hexadecimal
    // vectors. Here check the direct-output path and input/output aliasing.
    TigerDigest pid{};
    for (std::size_t i = 0; i < pid.size(); ++i) { pid[i] = static_cast<std::uint8_t>(i); }
    const auto wanted = tiger192(pid);
    require(wanted.has_value(), "Alias reference hash");
    require(tiger192_into(pid, pid) && pid == *wanted, "Direct hashing must preserve aliased input");
}
void canonical_domain() {
    const std::string valid(39, 'A');
    require(TigerCid::parse(valid)->text() == valid, "Canonical CID value");
    const auto pid = valid;
    const auto cid = cid_for(TigerDigest{});
    const auto result = verify_tiger_identity(cid, pid);
    require(std::holds_alternative<VerifiedIdentity>(result) &&
            std::get<VerifiedIdentity>(result).cid().text() == cid, "All-zero PID is valid protocol data");
    for (const auto length : {0U, 1U, 38U, 40U, 1024U, 65536U}) {
        const std::string bad(length, 'A');
        expect_error(bad, pid, IdentityError::invalid_cid);
        expect_error(cid, bad, IdentityError::invalid_pid);
    }
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    for (std::size_t position = 0; position < valid.size(); ++position) {
        for (unsigned byte = 0; byte < 256; ++byte) {
            if (alphabet.find(static_cast<char>(byte)) != std::string_view::npos) { continue; }
            auto bad = valid;
            bad[position] = static_cast<char>(byte);
            TigerDigest destination;
            destination.fill(0xa5);
            require(!decode_tiger_base32(bad, destination) && destination == TigerDigest{},
                    "Malformed decode must clear partial destination");
            expect_error(bad, pid, IdentityError::invalid_cid);
            expect_error(cid, bad, IdentityError::invalid_pid);
        }
    }
    for (std::size_t symbol = 0; symbol < alphabet.size(); ++symbol) {
        auto tail = valid;
        tail.back() = alphabet[symbol];
        const bool canonical = symbol % 8 == 0;
        require(TigerCid::parse(tail).has_value() == canonical, "Canonical 192-bit tail shape");
        TigerDigest destination;
        destination.fill(0xa5);
        require(decode_tiger_base32(tail, destination) == canonical, "Direct canonical tail check");
        if (!canonical) {
            require(destination == TigerDigest{}, "Bad tail retained decoded bytes");
            expect_error(cid, tail, IdentityError::invalid_pid);
        }
    }
    const auto unsupported = verify_tiger_identity(cid, pid, static_cast<SessionHash>(99));
    require(unsupported == std::variant<VerifiedIdentity, IdentityError>{IdentityError::unsupported_hash},
            "Unsupported algorithm must not fall back");
}
void pair_validation() {
    std::uint32_t random = 0xc1d917U;
    for (unsigned iteration = 0; iteration < 1000; ++iteration) {
        TigerDigest pid{};
        for (auto& byte : pid) {
            random = random * 1664525U + 1013904223U;
            byte = static_cast<std::uint8_t>(random >> 24U);
        }
        auto encoded_pid = tiger_base32(pid);
        const auto cid = cid_for(pid);
        auto result = verify_tiger_identity(cid, encoded_pid);
        require(std::holds_alternative<VerifiedIdentity>(result), "Valid pair rejected");
        const auto identity = std::get<VerifiedIdentity>(result);
        require(identity.cid().text() == cid && identity.cid() == *TigerCid::parse(cid), "Public proof CID");
        encoded_pid.assign(encoded_pid.size(), 'X');
        require(identity.cid().text() == cid, "Verified result aliases private input");
        pid[iteration % pid.size()] ^= 1;
        expect_error(cid, tiger_base32(pid), IdentityError::mismatch);
    }
    require(identity_status_code(IdentityError::invalid_cid) == "243" &&
            identity_status_code(IdentityError::invalid_pid) == "227" &&
            identity_status_code(IdentityError::mismatch) == "227" &&
            identity_status_code(IdentityError::unsupported_hash) == "247" &&
            identity_status_code(IdentityError::crypto_unavailable) == "210", "Fixed identity statuses");
}
void inf_proof_and_updates() {
    const auto pid = tiger_base32(TigerDigest{});
    const auto cid = cid_for(TigerDigest{});
    const auto source = *Sid::parse("AAAB");
    InfValidator validator("127.0.0.1");
    auto initial = validator.validate(parse("BINF AAAB NIalice ID" + cid + " PD" + pid), source, SessionHash::tiger192);
    require(std::holds_alternative<ClientInfo>(initial), "Validated identity must reach INF snapshot");
    const auto snapshot = std::get<ClientInfo>(std::move(initial));
    require(snapshot.identity() && snapshot.identity()->cid().text() == cid && !snapshot.field("PD"),
            "INF proof missing or private data retained");
    auto updated = validator.validate(parse("BINF AAAB DEchanged"), source, SessionHash::tiger192, &snapshot);
    require(std::holds_alternative<ClientInfo>(updated) &&
            std::get<ClientInfo>(updated).identity() == snapshot.identity(), "Ordinary update changed proof");
    auto alternate = TigerDigest{};
    alternate[0] = 1;
    const auto rejected = validator.validate(parse("BINF AAAB ID" + cid_for(alternate)), source,
                                             SessionHash::tiger192, &snapshot);
    require(rejected == std::variant<ClientInfo, InfError>{InfError::immutable_identity}, "CID reassignment accepted");
    for (const auto& token : snapshot.public_message(source).parameters) {
        require(token.substr(0, 2) != "PD", "Public snapshot exported PID");
    }
}
void ncdc_transcript() {
    auto pool = std::make_shared<SidPool>(4);
    ClientSession session(pool, "127.0.0.1");
    std::string text;
    require(static_cast<bool>(std::getline(std::cin, text)), "Missing ncdc SUP");
    require(!session.observe(Direction::from_client, parse(text)), "ncdc SUP rejected");
    std::cout << *session.support().initial_reply_line() << std::flush;
    require(static_cast<bool>(std::getline(std::cin, text)) && text == "SUP_SENT", "Missing SUP completion");
    require(!session.observe(Direction::to_client, *session.support().initial_reply()) && !session.prepare_sid(),
            "ncdc SID preparation");
    std::cout << *session.sid_reply_line() << std::flush;
    require(static_cast<bool>(std::getline(std::cin, text)) && text == "SID_SENT", "Missing SID completion");
    require(!session.observe(Direction::to_client, *session.sid_reply()), "ncdc SID completion");
    require(static_cast<bool>(std::getline(std::cin, text)), "Missing ncdc identity");
    auto message = parse(text);
    auto tampered = message;
    const auto id = std::find_if(tampered.parameters.begin(), tampered.parameters.end(), [](const auto& token) {
        return token.substr(0, 2) == "ID";
    });
    require(id != tampered.parameters.end(), "Missing client CID");
    auto cid = TigerCid::parse(std::string_view(*id).substr(2));
    require(cid.has_value(), "Invalid client CID shape");
    auto changed = cid->bytes();
    changed[0] ^= 1;
    *id = "ID" + tiger_base32(changed);
    require(session.observe(Direction::from_client, tampered) == ClientSessionError{InfError::cid_mismatch} &&
            !session.info() && session.state() == ConnectionState::identify, "Tampered identity mutated state");
    require(!session.observe(Direction::from_client, message), "Actual ncdc Tiger pair rejected");
    require(session.info() && session.info()->identity() && session.info()->identity()->cid() == *cid &&
            session.state() == ConnectionState::identify, "Verified identity unexpectedly admitted");
    for (const auto& token : session.info()->public_message(*Sid::parse(session.sid())).parameters) {
        require(token.substr(0, 2) != "PD", "ncdc PID exported");
    }
    session.disconnect();
    require(pool->size() == 0 && !session.info(), "Identity fixture cleanup");
    std::cout << "IDENTIFY TIGER_VERIFIED PID_DISCARDED\n" << std::flush;
}
} // namespace

int main(int argc, char** argv) {
    static_assert(!std::is_default_constructible_v<TigerCid>);
    static_assert(!std::is_default_constructible_v<VerifiedIdentity>);
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--ncdc-transcript") { ncdc_transcript(); return 0; }
        require(argc == 1, "Unexpected identity test arguments");
        reference_vectors(); canonical_domain(); pair_validation(); inf_proof_and_updates();
        std::cout << "PASS: Tiger CID/PID values, canonical encoding, pair verification and INF proof\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
