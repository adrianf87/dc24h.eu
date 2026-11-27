#include "dc24h/adc/inf.hpp"
#include "dc24h/adc/tiger_identity.hpp"

#include <gcrypt.h>
#include <iostream>
#include <stdexcept>

namespace {
enum class Fault { none, algorithm, context, read };
Fault fault = Fault::none;
unsigned opens = 0;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
dc24h::adc::Message parse(std::string_view text) {
    auto result = dc24h::adc::parse_line(text);
    require(std::holds_alternative<dc24h::adc::Message>(result), "Failure fixture syntax");
    return std::get<dc24h::adc::Message>(std::move(result));
}
} // namespace

// Executable-local GNU linker wrappers. Production targets and global crypto
// configuration are untouched; no injectable verifier is exposed to consumers.
extern "C" {
gcry_error_t __real_gcry_md_algo_info(int, int, void*, std::size_t*);
gcry_error_t __real_gcry_md_open(gcry_md_hd_t*, int, unsigned int);
unsigned char* __real_gcry_md_read(gcry_md_hd_t, int);
gcry_error_t __wrap_gcry_md_algo_info(int algorithm, int what, void* buffer, std::size_t* count) {
    if (fault == Fault::algorithm && algorithm == GCRY_MD_TIGER1 && what == GCRYCTL_TEST_ALGO) { return 1; }
    return __real_gcry_md_algo_info(algorithm, what, buffer, count);
}
gcry_error_t __wrap_gcry_md_open(gcry_md_hd_t* context, int algorithm, unsigned int flags) {
    ++opens;
    if (fault == Fault::context) { *context = nullptr; return 1; }
    return __real_gcry_md_open(context, algorithm, flags);
}
unsigned char* __wrap_gcry_md_read(gcry_md_hd_t context, int algorithm) {
    if (fault == Fault::read) { return nullptr; }
    return __real_gcry_md_read(context, algorithm);
}
}

int main() {
    using namespace dc24h::adc;
    try {
        const TigerDigest pid_bytes{};
        const auto digest = tiger192(pid_bytes);
        require(digest.has_value(), "Baseline Tiger provider unavailable");
        const auto cid = tiger_base32(*digest);
        const auto pid = tiger_base32(pid_bytes);
        const auto frame = parse("BINF AAAB NIalice ID" + cid + " PD" + pid);
        InfValidator validator("127.0.0.1");
        for (const auto injected : {Fault::algorithm, Fault::context, Fault::read}) {
            fault = injected;
            TigerDigest destination;
            destination.fill(0xa5);
            require(!tiger192_into(pid_bytes, destination) && destination == TigerDigest{}, "Failed hash retained output");
            require(verify_tiger_identity(cid, pid) ==
                    std::variant<VerifiedIdentity, IdentityError>{IdentityError::crypto_unavailable}, "Failure accepted identity");
            require(validator.validate(frame, *Sid::parse("AAAB"), SessionHash::tiger192) ==
                    std::variant<ClientInfo, InfError>{InfError::crypto_unavailable}, "INF hid provider failure");
            auto pool = std::make_shared<SidPool>(1);
            ClientSession session(pool, "127.0.0.1");
            require(!session.observe(Direction::from_client, parse("HSUP ADBASE ADTIGR")), "Failure fixture SUP");
            require(!session.observe(Direction::to_client, *session.support().initial_reply()), "Failure fixture ISUP");
            require(!session.prepare_sid() && !session.observe(Direction::to_client, *session.sid_reply()), "Failure fixture SID");
            require(session.observe(Direction::from_client, frame) == ClientSessionError{InfError::crypto_unavailable} &&
                    !session.info() && session.state() == ConnectionState::identify && pool->size() == 1,
                    "Provider failure committed session state");
            const auto previous_opens = opens;
            require(verify_tiger_identity(cid, "bad") ==
                    std::variant<VerifiedIdentity, IdentityError>{IdentityError::invalid_pid} && opens == previous_opens,
                    "Malformed PID opened a hash context");
            fault = Fault::none;
            require(!session.observe(Direction::from_client, frame) && session.info()->identity(), "Recovery after failure");
            session.disconnect();
            require(pool->size() == 0, "Failed/recovered session leaked reservation");
        }
        std::cout << "PASS: algorithm/context/read failures, cleared buffers and transactional identity recovery\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
