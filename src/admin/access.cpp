#include "dc24h/admin/access.hpp"

#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/repository.hpp"
#include "dc24h/admin/core.hpp"
#include "dc24h/security/roles.hpp"

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <limits>
#include <new>
#include <optional>
#include <utility>

namespace dc24h::admin {
namespace {
constexpr auto maximum = std::uint64_t(std::numeric_limits<std::int64_t>::max());
[[noreturn]] void fail(ErrorKind kind) { throw Error(kind); }
bool valid_id(std::uint64_t id) noexcept { return id && id <= maximum; }

int nibble(char c) noexcept {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}
bool decode_grant(std::string_view text, std::array<unsigned char, 32>& out) noexcept {
    if (text.size() != 64) { return false; }
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = nibble(text[i * 2]);
        const int lo = nibble(text[i * 2 + 1]);
        if (hi < 0 || lo < 0) { return false; }
        out[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return true;
}
template <std::size_t N>
std::string hex_bytes(const std::array<unsigned char, N>& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text(bytes.size() * 2, '\0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        text[i * 2] = digits[bytes[i] >> 4];
        text[i * 2 + 1] = digits[bytes[i] & 0x0f];
    }
    return text;
}
bool decode_session(std::string_view text, std::array<unsigned char, 16>& out) noexcept {
    if (text.size() != 32) { return false; }
    for (std::size_t i = 0; i < out.size(); ++i) {
        const char hi_c = text[i * 2];
        const char lo_c = text[i * 2 + 1];
        if (hi_c < '0' || (hi_c > '9' && hi_c < 'a') || hi_c > 'f' || lo_c < '0' || (lo_c > '9' && lo_c < 'a') || lo_c > 'f') {
            return false;
        }
        out[i] = static_cast<unsigned char>((nibble(hi_c) << 4) | nibble(lo_c));
    }
    return true;
}

enum class Gate { discovery, accounts, roles, password, bans, audit, contract, configuration };
bool allows_gate(accounts::Role role, Gate gate) noexcept {
    using security::Permission;
    switch (gate) {
    case Gate::discovery: return true;
    case Gate::contract: return true;
    case Gate::configuration: return security::allows(role, Permission::manage_configuration);
    case Gate::accounts: return security::allows(role, Permission::manage_accounts);
    case Gate::roles: return security::allows(role, Permission::manage_roles);
    case Gate::password: return security::allows(role, Permission::change_own_password);
    case Gate::bans:
        return security::allows(role, Permission::ban_users) && security::allows(role, Permission::manage_blocks);
    case Gate::audit: return security::allows(role, Permission::read_audit);
    }
    return false;
}
bool canonical_suffix(std::string_view action, Gate& gate) noexcept {
    if (action == "activate" || action == "disable" || action == "reset") { gate = Gate::accounts; return true; }
    if (action == "role") { gate = Gate::roles; return true; }
    if (action == "password") { gate = Gate::password; return true; }
    return false;
}
std::optional<Gate> classify(std::string_view method, std::string_view target) noexcept {
    constexpr std::string_view accounts = "/api/v1/accounts";
    if (target == "/api/v1/admin" || target == "/api/v1/management") { return Gate::discovery; }
    if (target == "/api/v1/management/authorize") { return Gate::contract; }
    if (target == "/api/v1/management/configuration" && (method == "GET" || method == "POST")) {
        return Gate::configuration;
    }
    if (target == "/api/v1/audit") { return Gate::audit; }
    if (target.starts_with("/api/v1/bans")) { return Gate::bans; }
    if (target == "/api/v1/admin/access" || !target.starts_with(accounts)) { return std::nullopt; }
    if (method == "GET") { return Gate::accounts; }
    if (method != "POST") { return Gate::discovery; }
    const auto tail = target.substr(accounts.size());
    if (tail.empty()) { return Gate::accounts; }
    if (tail.front() != '/') { return Gate::discovery; }
    const auto slash = tail.find('/', 1);
    if (slash == std::string_view::npos) { return Gate::discovery; }
    Gate gate = Gate::discovery;
    if (!canonical_suffix(tail.substr(slash + 1), gate)) { return Gate::discovery; }
    return gate;
}
}

void AccessGate::Slot::clear() noexcept {
    OPENSSL_cleanse(secret.data(), secret.size());
    OPENSSL_cleanse(csrf.data(), csrf.size());
    OPENSSL_cleanse(session.data(), session.size());
    used = false;
    actor = 0;
    expires = {};
    window_started = {};
    window_count = 0;
}
AccessGate::Slot::Slot(const Slot& other) noexcept
    : used(other.used), secret(other.secret), csrf(other.csrf), session(other.session), actor(other.actor),
      expires(other.expires), window_started(other.window_started), window_count(other.window_count) {}
AccessGate::Slot& AccessGate::Slot::operator=(const Slot& other) noexcept {
    if (this != &other) {
        clear();
        used = other.used;
        secret = other.secret;
        csrf = other.csrf;
        session = other.session;
        actor = other.actor;
        expires = other.expires;
        window_started = other.window_started;
        window_count = other.window_count;
    }
    return *this;
}
AccessGate::Slot::Slot(Slot&& other) noexcept { *this = std::move(other); }
AccessGate::Slot& AccessGate::Slot::operator=(Slot&& other) noexcept {
    if (this != &other) {
        clear();
        used = other.used;
        secret = other.secret;
        csrf = other.csrf;
        session = other.session;
        actor = other.actor;
        expires = other.expires;
        window_started = other.window_started;
        window_count = other.window_count;
        other.clear();
    }
    return *this;
}
AccessGate::Slot::~Slot() { clear(); }

AccessGate::AccessGate(database::Options options) : options_(std::move(options)) {}

std::size_t AccessGate::choose_slot(std::uint64_t actor) const noexcept {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].used && slots_[i].actor == actor) { return i; }
    }
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (!slots_[i].used) { return i; }
    }
    std::size_t chosen = 0;
    for (std::size_t i = 1; i < slots_.size(); ++i) {
        if (slots_[i].expires < slots_[chosen].expires) { chosen = i; }
    }
    return chosen;
}

IssuedAccess AccessGate::issue(std::uint64_t actor, std::string_view proof) {
    if (!valid_id(actor) || proof.empty() || proof.size() > 1024) { fail(ErrorKind::invalid_input); }
    try {
        database::Connection connection(options_);
        accounts::AccountRepository repository(connection);
        const auto id = accounts::AccountId::from_value(actor);
        if (!id) { fail(ErrorKind::invalid_input); }
        const auto stored = repository.find_by_id(*id);
        if (!stored) { fail(ErrorKind::not_found); }
        if (stored->account.status() != accounts::Status::active) { fail(ErrorKind::forbidden); }
        const auto verdict = accounts::PasswordSecurityService(connection).verify(stored->account.id(), proof);
        if (verdict == accounts::PasswordVerification::unavailable) { fail(ErrorKind::unavailable); }
        if (verdict != accounts::PasswordVerification::matched) { fail(ErrorKind::forbidden); }
        std::array<unsigned char, 32> bytes{};
        std::array<unsigned char, 32> csrf_bytes{};
        std::array<unsigned char, 16> session_bytes{};
        struct Wipe {
            std::array<unsigned char, 32>& raw;
            std::array<unsigned char, 32>& csrf_raw;
            std::array<unsigned char, 16>& session_raw;
            ~Wipe() {
                OPENSSL_cleanse(raw.data(), raw.size());
                OPENSSL_cleanse(csrf_raw.data(), csrf_raw.size());
                OPENSSL_cleanse(session_raw.data(), session_raw.size());
            }
        } wipe{bytes, csrf_bytes, session_bytes};
        if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1 ||
            RAND_bytes(csrf_bytes.data(), static_cast<int>(csrf_bytes.size())) != 1 ||
            RAND_bytes(session_bytes.data(), static_cast<int>(session_bytes.size())) != 1) { fail(ErrorKind::unavailable); }
        auto& slot = slots_[choose_slot(actor)];
        slot.clear();
        slot.secret = bytes;
        slot.csrf = csrf_bytes;
        slot.session = session_bytes;
        slot.used = true;
        slot.actor = actor;
        slot.expires = std::chrono::steady_clock::now() + access_grant_ttl;
        IssuedAccess issued;
        issued.id = actor;
        issued.role = std::string(accounts::role_key(stored->account.role()));
        issued.status = std::string(accounts::status_key(stored->account.status()));
        issued.grant = hex_bytes(bytes);
        issued.session = hex_bytes(session_bytes);
        issued.csrf = hex_bytes(csrf_bytes);
        return issued;
    } catch (const accounts::PasswordSecurityError& error) {
        fail(error.kind() == accounts::PasswordSecurityErrorKind::invalid_password ? ErrorKind::invalid_input : ErrorKind::unavailable);
    } catch (const accounts::RepositoryError&) { fail(ErrorKind::unavailable);
    } catch (const database::Error& error) { fail(error.outcome_unknown() ? ErrorKind::outcome_unknown : ErrorKind::unavailable);
    } catch (const Error&) { throw; }
    catch (const std::bad_alloc&) { throw; }
    catch (...) { fail(ErrorKind::unavailable); }
}

GrantCheck AccessGate::authorize(std::string_view token, std::string_view method, std::string_view target,
    std::string_view session, std::string_view csrf) {
    std::array<unsigned char, 32> raw{};
    struct Wipe { std::array<unsigned char, 32>& bytes; ~Wipe() { OPENSSL_cleanse(bytes.data(), bytes.size()); } } wipe{raw};
    if (!decode_grant(token, raw)) { return GrantCheck::missing; }
    const auto now = std::chrono::steady_clock::now();
    int match = -1;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        const int equal = CRYPTO_memcmp(slots_[i].secret.data(), raw.data(), raw.size());
        if (equal == 0 && slots_[i].used && slots_[i].expires > now) { match = static_cast<int>(i); }
    }
    if (match < 0) { return GrantCheck::missing; }
    auto& slot = slots_[static_cast<std::size_t>(match)];
    if (slot.window_started.time_since_epoch().count() == 0 || now >= slot.window_started + session_request_window) {
        slot.window_started = now;
        slot.window_count = 0;
    }
    if (slot.window_count >= session_request_limit) { return GrantCheck::rate_limited; }
    ++slot.window_count;
    std::array<unsigned char, 32> csrf_raw{};
    std::array<unsigned char, 16> session_raw{};
    struct SecretWipe {
        std::array<unsigned char, 32>& csrf_bytes;
        std::array<unsigned char, 16>& session_bytes;
        ~SecretWipe() {
            OPENSSL_cleanse(csrf_bytes.data(), csrf_bytes.size());
            OPENSSL_cleanse(session_bytes.data(), session_bytes.size());
        }
    } secret_wipe{csrf_raw, session_raw};
    const bool csrf_ok = decode_grant(csrf, csrf_raw);
    const bool session_ok = decode_session(session, session_raw);
    const int csrf_diff = csrf_ok ? CRYPTO_memcmp(slot.csrf.data(), csrf_raw.data(), csrf_raw.size()) : 1;
    const int session_diff = session_ok ? CRYPTO_memcmp(slot.session.data(), session_raw.data(), session_raw.size()) : 1;
    // Binding is checked before the role reload, for reads and writes.
    if (!csrf_ok || !session_ok || csrf_diff != 0 || session_diff != 0) { return GrantCheck::csrf_required; }
    const auto gate = classify(method, target);
    if (!gate) { return GrantCheck::forbidden; }
    const auto actor = slots_[static_cast<std::size_t>(match)].actor;
    try {
        database::Connection connection(options_);
        accounts::AccountRepository repository(connection);
        const auto id = accounts::AccountId::from_value(actor);
        const auto stored = id ? repository.find_by_id(*id) : std::optional<accounts::StoredAccount>{};
        if (!stored) {
            slots_[static_cast<std::size_t>(match)].clear();
            return GrantCheck::not_found;
        }
        if (stored->account.status() != accounts::Status::active || !allows_gate(stored->account.role(), *gate)) {
            return GrantCheck::forbidden;
        }
        return GrantCheck::accepted;
    } catch (const database::Error& error) {
        return error.outcome_unknown() ? GrantCheck::outcome_unknown : GrantCheck::unavailable;
    } catch (const accounts::RepositoryError&) { return GrantCheck::unavailable;
    } catch (const std::bad_alloc&) { throw; }
    catch (...) { return GrantCheck::unavailable; }
}

std::optional<accounts::Role> AccessGate::actor_role(std::string_view token) {
    std::array<unsigned char, 32> raw{};
    struct Wipe { std::array<unsigned char, 32>& bytes; ~Wipe() { OPENSSL_cleanse(bytes.data(), bytes.size()); } } wipe{raw};
    if (!decode_grant(token, raw)) { return std::nullopt; }
    const auto now = std::chrono::steady_clock::now();
    int match = -1;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        const int equal = CRYPTO_memcmp(slots_[i].secret.data(), raw.data(), raw.size());
        if (equal == 0 && slots_[i].used && slots_[i].expires > now) { match = static_cast<int>(i); }
    }
    if (match < 0) { return std::nullopt; }
    const auto actor = slots_[static_cast<std::size_t>(match)].actor;
    try {
        database::Connection connection(options_);
        accounts::AccountRepository repository(connection);
        const auto id = accounts::AccountId::from_value(actor);
        const auto stored = id ? repository.find_by_id(*id) : std::optional<accounts::StoredAccount>{};
        if (!stored || stored->account.status() != accounts::Status::active) {
            if (!stored) { slots_[static_cast<std::size_t>(match)].clear(); }
            return std::nullopt;
        }
        return stored->account.role();
    } catch (const database::Error& error) {
        throw Error(error.outcome_unknown() ? ErrorKind::outcome_unknown : ErrorKind::unavailable);
    } catch (const accounts::RepositoryError&) { throw Error(ErrorKind::unavailable);
    } catch (const std::bad_alloc&) { throw; }
    catch (const Error&) { throw; }
    catch (...) { throw Error(ErrorKind::unavailable); }
}

std::array<PermissionFact, 17> permission_facts(accounts::Role role) noexcept {
    std::array<PermissionFact, 17> facts{};
    for (std::size_t i = 0; i < security::permission_catalog.size(); ++i) {
        const auto permission = security::permission_catalog[i];
        facts[i] = PermissionFact{security::permission_key(permission), security::allows(role, permission), security::permission_implemented(permission)};
    }
    return facts;
}
bool catalog_allowed(accounts::Role role, std::string_view permission) noexcept {
    if (permission.empty()) { return true; }
    if (permission == "users.ban+blocks.manage") {
        return security::allows(role, security::Permission::ban_users) && security::allows(role, security::Permission::manage_blocks);
    }
    const auto parsed = security::parse_permission(permission);
    return parsed && security::allows(role, *parsed);
}
bool route_allowed(accounts::Role role, std::string_view method, std::string_view target) noexcept {
    const auto gate = classify(method, target);
    return gate && allows_gate(role, *gate);
}
} // namespace dc24h::admin
