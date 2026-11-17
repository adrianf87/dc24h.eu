#include "dc24h/accounts/account.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace { int fail_after = -1; }
[[gnu::noinline]] void* operator new(std::size_t n) {
    if (fail_after == 0) { fail_after = -1; throw std::bad_alloc(); }
    if (fail_after > 0) { --fail_after; }
    if (auto* p = std::malloc(n ? n : 1)) { return p; }
    throw std::bad_alloc();
}
[[gnu::noinline]] void* operator new[](std::size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using namespace dc24h::accounts;
void require(bool value, std::source_location where = std::source_location::current()) {
    if (!value) { throw std::runtime_error("Account Model assertion failed at line " + std::to_string(where.line())); }
}
AccountData fixture() { return {42, "Zażółć_gęślą", Role::registered, Status::disabled, {100, 200, 150}}; }
Account checked(const AccountData& data) { return std::get<Account>(Account::create(data)); }
void rejects(const std::variant<Account, AccountError>& result, AccountError expected) {
    require(std::holds_alternative<AccountError>(result) && std::get<AccountError>(result) == expected);
}
void ids_and_names() {
    static_assert(!std::is_default_constructible_v<AccountId>);
    static_assert(!std::is_constructible_v<AccountId, std::uint64_t>);
    static_assert(!std::is_default_constructible_v<AccountName>);
    static_assert(!std::is_constructible_v<AccountName, std::string_view>);
    static_assert(!std::is_default_constructible_v<Account>);
    static_assert(std::is_nothrow_move_constructible_v<Account>);
    static_assert(std::is_nothrow_move_assignable_v<Account>);
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    require(!AccountId::from_value(0) && AccountId::from_value(1)->value() == 1);
    require(AccountId::from_value(maximum)->value() == maximum);
    require(!AccountId::from_value(maximum + 1) && !AccountId::from_value(std::numeric_limits<std::uint64_t>::max()));
    require(AccountId::from_value(1) != AccountId::from_value(2));
    for (const auto& value : std::vector<std::string>{"", "a b", " x", "x ", "\t", "\n", "\r", "\x7f", "\x80",
            "\xc0\xaf", "\xe2\x82", "\xed\xa0\x80", "\xf4\x90\x80\x80", std::string("a\0b", 3),
            "e\xcc\x81", "a\xc2\xa0", "a\xe2\x80\x83", "a\xe2\x80\x8b", "a\xe2\x80\xae",
            "a\xe2\x80\xa8", "a\xe2\x80\xa9", "\xef\xbb\xbf", std::string(65, 'x')}) {
        require(!AccountName::create(value));
        auto data = fixture(); data.name = value;
        rejects(Account::create(data), AccountError::invalid_name);
    }
    std::string unicode;
    for (unsigned n = 0; n < 32; ++n) { unicode += "é"; }
    require(unicode.size() == max_account_name_bytes && AccountName::create(unicode));
    require(!AccountName::create(unicode + "é"));
    for (const auto& value : {"a", "A", "Żółw", "日本語", "😺", "name_1.-", "x'OR'1=1;--"}) {
        const auto name = AccountName::create(value);
        require(name && name->valid() && name->text() == value);
    }
    require(AccountName::create("Alice") != AccountName::create("alice"));
    std::string input(64, 'x');
    auto name = *AccountName::create(input);
    input.assign(64, 'y');
    require(name.text() == std::string(64, 'x'));
    auto moved = std::move(name);
    require(moved.valid() && !name.valid());
    name = *AccountName::create("reused");
    require(name.text() == "reused" && moved.text() == std::string(64, 'x'));
}
void stable_keys() {
    const std::vector<std::pair<Role, std::string_view>> roles{
        {Role::registered, "registered"}, {Role::moderator, "moderator"}, {Role::administrator, "administrator"}, {Role::vip,"vip"}, {Role::operator_,"operator"}, {Role::owner,"owner"}, {Role::guest,"guest"}};
    const std::vector<std::pair<Status, std::string_view>> statuses{{Status::disabled, "disabled"}, {Status::active, "active"}};
    for (const auto& [role, key] : roles) { require(role_key(role) == key && parse_role(key) == role); }
    for (const auto& [status, key] : statuses) { require(status_key(status) == key && parse_status(key) == status); }
    require(role_key(static_cast<Role>(-1)).empty() && role_key(static_cast<Role>(100)).empty());
    require(status_key(static_cast<Status>(-1)).empty() && status_key(static_cast<Status>(100)).empty());
    for (const auto key : {"", "Registered", "admin", "Guest", " moderator", "active ", "aktywne"}) {
        require(!parse_role(key) && !parse_status(key));
    }
    require(!parse_role(std::string("registered\0x", 12)) && !parse_status(std::string("active\0x", 8)));
}
void importing() {
    const AccountData defaults;
    require(defaults.id == 0 && defaults.role == Role::registered && defaults.status == Status::disabled);
    rejects(Account::create(defaults), AccountError::invalid_id);
    auto data = fixture();
    auto account = checked(data);
    require(account.valid() && account.snapshot() == data);
    require(account.id().value() == data.id && account.name().text() == data.name &&
        account.role() == data.role && account.status() == data.status && account.times() == data.times);
    data.name = "changed"; data.times.created_at = 0;
    require(account.name().text() == "Zażółć_gęślą" && account.times().created_at == 100);
    auto exported = account.snapshot(); exported.name = "export"; exported.role = Role::administrator;
    require(account.role() == Role::registered && account.name().text() != exported.name);
    require(checked(account.snapshot()) == account);
    data = fixture(); data.id = 0; rejects(Account::create(data), AccountError::invalid_id);
    data = fixture(); data.id = std::numeric_limits<std::uint64_t>::max(); rejects(Account::create(data), AccountError::invalid_id);
    data = fixture(); data.role = Role::guest; rejects(Account::create(data), AccountError::invalid_role);
    data = fixture(); data.role = static_cast<Role>(99); rejects(Account::create(data), AccountError::invalid_role);
    data = fixture(); data.status = static_cast<Status>(99); rejects(Account::create(data), AccountError::invalid_status);
    for (const auto role : {Role::registered, Role::moderator, Role::administrator}) {
        for (const auto status : {Status::disabled, Status::active}) {
            data = fixture(); data.role = role; data.status = status;
            require(checked(data).snapshot() == data);
        }
    }
    for (const Times times : {Times{0, 0, {}}, Times{0, 0, 0}, Times{100, 200, {}}, Times{100, 200, 100},
            Times{100, 200, 200}, Times{max_timestamp, max_timestamp, max_timestamp}}) {
        data = fixture(); data.times = times; require(checked(data).times() == times);
    }
    for (const Times times : {Times{-1, 0, {}}, Times{0, -1, {}}, Times{200, 100, {}}, Times{100, 200, 99},
            Times{100, 200, 201}, Times{0, 0, -1}, Times{0, max_timestamp + 1, {}},
            Times{max_timestamp + 1, max_timestamp + 1, {}}, Times{0, max_timestamp, max_timestamp + 1},
            Times{std::numeric_limits<Timestamp>::min(), std::numeric_limits<Timestamp>::max(), {}}}) {
        data = fixture(); data.times = times; rejects(Account::create(data), AccountError::invalid_times);
    }
}
void updates_and_ownership() {
    const auto original = checked(fixture());
    const auto saved = original.snapshot();
    auto renamed = std::get<Account>(original.with_name("NowaNazwa", 201));
    require(renamed.id() == original.id() && renamed.name().text() == "NowaNazwa" &&
        renamed.role() == original.role() && renamed.status() == original.status() &&
        renamed.times() == Times{100, 201, 150} && original.snapshot() == saved);
    auto elevated = std::get<Account>(renamed.with_access(Role::administrator, Status::active, 202));
    require(elevated.id() == original.id() && elevated.name() == renamed.name() &&
        elevated.role() == Role::administrator && elevated.status() == Status::active &&
        elevated.times() == Times{100, 202, 150} && renamed.role() == Role::registered);
    auto logged = std::get<Account>(elevated.with_last_login(203));
    require(logged.times() == Times{100, 203, 203} && elevated.times().last_login_at == 150);
    auto disabled = std::get<Account>(logged.with_access(Role::registered, Status::disabled, 204));
    require(disabled.times() == Times{100, 204, 203} && disabled.status() == Status::disabled);
    require(std::get<Account>(disabled.with_last_login(204)).times().last_login_at == 204);
    for (Timestamp when : std::vector<Timestamp>{-1, 199, max_timestamp + 1}) {
        rejects(original.with_name("valid", when), AccountError::invalid_times);
        rejects(original.with_access(Role::registered, Status::active, when), AccountError::invalid_times);
        rejects(original.with_last_login(when), AccountError::invalid_times);
        require(original.snapshot() == saved);
    }
    rejects(original.with_name("invalid name", 200), AccountError::invalid_name);
    rejects(original.with_access(static_cast<Role>(99), Status::active, 200), AccountError::invalid_role);
    rejects(original.with_access(Role::registered, static_cast<Status>(99), 200), AccountError::invalid_status);
    require(std::get<Account>(original.with_name(original.name().text(), 200)) == original);
    const auto latest = std::get<Account>(original.with_last_login(max_timestamp));
    require(latest.times() == Times{100, max_timestamp, max_timestamp});
    rejects(latest.with_name("valid", max_timestamp - 1), AccountError::invalid_times);
    auto source = original;
    auto moved = std::move(source);
    require(moved == original && moved.valid() && !source.valid());
    rejects(Account::create(source.snapshot()), AccountError::invalid_name);
    rejects(source.with_name("recover", 200), AccountError::invalid_name);
    rejects(source.with_access(Role::registered, Status::active, 200), AccountError::invalid_name);
    rejects(source.with_last_login(200), AccountError::invalid_name);
    source = moved;
    require(source == original && source.valid());
    source = source;
    require(source == original);
    auto* alias = &source;
    source = std::move(*alias);
    require(source == original && source.valid());
}
void allocation_failures() {
    auto input = fixture(); input.name.assign(64, 'x');
    const auto original = checked(input);
    auto oversized = input; oversized.name.assign(10000, 'x');
    fail_after = 0;
    const auto rejected = Account::create(oversized);
    fail_after = -1;
    rejects(rejected, AccountError::invalid_name);
    auto target_data = fixture(); target_data.id = 99; target_data.name = "target";
    auto target = checked(target_data);
    const auto saved = target.snapshot();
    bool saw_failure = false, saw_success = false;
    for (int position = 0; position < 4; ++position) {
        bool failed = false;
        fail_after = position;
        try { target = original; } catch (const std::bad_alloc&) { failed = true; }
        fail_after = -1;
        if (failed) { saw_failure = true; require(target.snapshot() == saved); }
        else { saw_success = true; require(target == original); }
        target = checked(target_data);
    }
    require(saw_failure && saw_success);
    for (unsigned operation = 0; operation < 5; ++operation) {
        bool failed = false;
        fail_after = 0;
        try {
            switch (operation) {
                case 0: (void)Account::create(input); break;
                case 1: (void)original.snapshot(); break;
                case 2: (void)original.with_name(input.name, 201); break;
                case 3: (void)original.with_access(Role::moderator, Status::active, 201); break;
                case 4: (void)original.with_last_login(201); break;
            }
        } catch (const std::bad_alloc&) { failed = true; }
        fail_after = -1;
        require(failed && original.snapshot() == input);
    }
}
}
int main() {
    try { ids_and_names(); stable_keys(); importing(); updates_and_ownership(); allocation_failures(); }
    catch (const std::exception& error) { fail_after = -1; std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Account Model bounds, Unicode, timestamps, immutable updates, ownership and allocation failures passed\n";
}
