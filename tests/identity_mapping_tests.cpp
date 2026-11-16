#include "dc24h/accounts/identity_mapping.hpp"
#include "dc24h/accounts/repository.hpp"

#include <barrier>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <new>
#include <source_location>
#include <thread>

namespace { thread_local int fail_after = -1; }
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
using dc24h::database::Connection;
using dc24h::database::Options;
void require(bool ok, std::source_location where = std::source_location::current()) {
    if (!ok) { throw std::runtime_error("Identity mapping assertion at line " + std::to_string(where.line())); }
}
template<class F> void rejects(MappingErrorKind kind, F&& action,
    std::source_location where = std::source_location::current()) {
    try { action(); }
    catch (const MappingError& error) { require(error.kind() == kind,where); return; }
    require(false,where);
}
std::string cid(char first = 'A') { auto value = std::string(39,'A'); value[0] = first; return value; }
IdentityKey key(std::string_view name, char first = 'A') {
    auto value = IdentityKey::create(name,cid(first)); require(value.has_value()); return std::move(*value);
}
void unit() {
    const auto public_id = cid();
    require(IdentityKey::create("Zażółć",public_id).has_value());
    require(IdentityKey::create(std::string(64,'x'),public_id).has_value());
    for (const auto& name : {std::string(), std::string(65,'x'), std::string("a b"), std::string("a\n"),
                            std::string("a\0b",3), std::string("e\xcc\x81"), std::string("\xff"),
                            std::string("a\xe2\x80\x8b")}) {
        require(!IdentityKey::create(name,public_id));
    }
    for (const auto& value : {std::string(38,'A'), std::string(40,'A'), std::string(39,'a'),
                             std::string(38,'A')+'B', std::string(38,'A')+'7', std::string(39,'0')}) {
        require(!IdentityKey::create("Nick",value));
    }
    auto original = key("Nick"); const auto copied = original;
    auto moved = std::move(original);
    require(moved == copied && !original.valid());
    original = moved; require(original == copied);
    require(key("Nick") != key("nick"));
    require(key("Nick",'A') != key("Nick",'B'));
    for (const auto kind : {MappingErrorKind::invalid_identity,MappingErrorKind::invalid_time,
                           MappingErrorKind::account_missing,MappingErrorKind::conflict,
                           MappingErrorKind::invalid_record,MappingErrorKind::schema}) {
        MappingError error(kind); require(error.kind() == kind);
        require(std::string_view(error.what()).find(public_id) == std::string_view::npos);
    }
}
void allocation_failures(const Options& options) {
    Connection observer(options); AccountRepository accounts(observer); IdentityRepository reader(observer);
    const auto account = accounts.insert({"MappingAllocation"}).account.id();
    const auto input = key(std::string(64,'z'),'Z');
    int failures = 0; bool saved = false;
    for (int index = 0; index < 512 && !saved; ++index) {
        Connection c(options); IdentityRepository repository(c);
        fail_after = index;
        try { (void)repository.bind(account,input,0); fail_after = -1; saved = true; }
        catch (const std::bad_alloc&) { fail_after = -1; ++failures; }
        require(!c.in_transaction());
        require(reader.find_by_account(account).has_value() == saved);
    }
    require(saved && failures > 30);
}
void integration(const Options& options) {
    Connection c(options); AccountRepository accounts(c); IdentityRepository repository(c);
    const auto first = accounts.insert({"MappingAccount"});
    const auto second = accounts.insert({"MappingOther"});
    const auto input = key("Zażółć'\\;--");
    require(!repository.find_by_account(first.account.id()));
    const auto stored = repository.bind(first.account.id(),input,0);
    require(stored == StoredIdentity{first.account.id(),input,0});
    require(repository.find_by_account(first.account.id()) == stored);
    require(repository.find_by_nickname(input.nickname()) == stored);
    require(repository.find_by_cid(input.cid()) == stored);
    require(repository.resolve(input) == stored);
    require(!repository.resolve(key(input.nickname(),'B')));
    require(!repository.resolve(key("OtherNick")));
    require(accounts.find_by_id(first.account.id()) == first); // No role/status/login changes.
    const auto other = repository.bind(second.account.id(),key("OtherNick",'B'),max_timestamp);
    require(!repository.resolve(key(input.nickname(),'B'))); // Two valid rows cannot be cross-combined.
    rejects(MappingErrorKind::conflict,[&] { (void)repository.bind(first.account.id(),input,0); });
    const auto target = accounts.insert({"MappingReadOnlyTarget"}).account.id();
    rejects(MappingErrorKind::conflict,[&] { (void)repository.bind(target,input,0); });
    rejects(MappingErrorKind::conflict,[&] { (void)repository.bind(target,key("UniqueNick"),0); });
    rejects(MappingErrorKind::conflict,[&] { (void)repository.bind(target,key(input.nickname(),'C'),0); });
    require(!c.in_transaction() && !repository.find_by_account(target));
    rejects(MappingErrorKind::account_missing,[&] {
        (void)repository.bind(*AccountId::from_value(9223372036854775807ULL),key("Missing",'M'),0);
    });
    for (const auto time : {Timestamp{-1},max_timestamp+1}) {
        rejects(MappingErrorKind::invalid_time,[&] { (void)repository.bind(target,key("InvalidTime",'T'),time); });
    }
    auto moved = key("Moved",'D'); auto owner = std::move(moved); (void)owner;
    rejects(MappingErrorKind::invalid_identity,[&] { (void)repository.bind(target,moved,0); });
    rejects(MappingErrorKind::invalid_identity,[&] { (void)repository.resolve(moved); });
    rejects(MappingErrorKind::invalid_identity,[&] { (void)repository.find_by_nickname("bad name"); });
    { auto transaction = c.transaction();
      try { (void)repository.resolve(input); require(false); }
      catch (const dc24h::database::Error& error) { require(error.kind() == dc24h::database::ErrorKind::transaction); }
      transaction.rollback(); }
    { Connection other_connection(options); IdentityRepository reader(other_connection);
      require(reader.find_by_account(first.account.id()) == stored); }
    std::exception_ptr thread_error;
    std::jthread wrong_thread([&] {
        try { (void)repository.resolve(input); require(false); }
        catch (const dc24h::database::Error& error) {
            if (error.kind() != dc24h::database::ErrorKind::wrong_thread) { thread_error = std::current_exception(); }
        } catch (...) { thread_error = std::current_exception(); }
    });
    wrong_thread.join(); if (thread_error) { std::rethrow_exception(thread_error); }
    const auto upper = accounts.insert({"MappingUpper"}).account.id();
    const auto lower = accounts.insert({"MappingLower"}).account.id();
    (void)repository.bind(upper,key("Alice",'E'),1);
    (void)repository.bind(lower,key("alice",'F'),1);
    require(repository.find_by_nickname("Alice")->account_id == upper);
    require(repository.find_by_nickname("alice")->account_id == lower);
    require(!repository.find_by_nickname("ALICE"));
    const auto race_a = accounts.insert({"MappingRaceA"}).account.id();
    const auto race_b = accounts.insert({"MappingRaceB"}).account.id();
    std::barrier gate(2); int won[2]{}; std::exception_ptr errors[2];
    auto race = [&](int index) {
        try {
            Connection rival(options); IdentityRepository writer(rival);
            gate.arrive_and_wait();
            try { (void)writer.bind(index == 0 ? race_a : race_b,key("RaceNick",'R'),2); won[index] = 1; }
            catch (const MappingError& error) { require(error.kind() == MappingErrorKind::conflict); }
            require(!rival.in_transaction());
        } catch (...) { errors[index] = std::current_exception(); }
    };
    std::jthread a(race,0), b(race,1); a.join(); b.join();
    for (const auto& error : errors) { if (error) { std::rethrow_exception(error); } }
    require(won[0]+won[1] == 1 && repository.resolve(key("RaceNick",'R')).has_value());
    c.execute("ALTER TABLE dc24h_account_identities DROP INDEX dc24h_identities_cid");
    rejects(MappingErrorKind::schema,[&] { (void)repository.resolve(input); });
    c.execute("ALTER TABLE dc24h_account_identities ADD UNIQUE KEY dc24h_identities_cid(cid)");
    c.execute("ALTER TABLE dc24h_account_identities DROP FOREIGN KEY dc24h_identities_account");
    rejects(MappingErrorKind::schema,[&] { (void)repository.resolve(input); });
    c.execute("ALTER TABLE dc24h_account_identities ADD CONSTRAINT dc24h_identities_account FOREIGN KEY(account_id) REFERENCES dc24h_accounts(id) ON UPDATE RESTRICT ON DELETE RESTRICT");
    c.execute("SET check_constraint_checks=OFF");
    c.execute("UPDATE dc24h_account_identities SET cid='PrivateInvalidCid' WHERE account_id=?",{std::to_string(first.account.id().value())});
    rejects(MappingErrorKind::invalid_record,[&] { (void)repository.find_by_account(first.account.id()); });
    c.execute("UPDATE dc24h_account_identities SET cid=? WHERE account_id=?",{input.cid().text(),std::to_string(first.account.id().value())});
    c.execute("SET check_constraint_checks=ON");
    require(repository.resolve(input) == stored && repository.resolve(other.identity) == other);
    try { c.execute("DELETE FROM dc24h_accounts WHERE id=?",{std::to_string(first.account.id().value())}); require(false); }
    catch (const dc24h::database::Error& error) { require(error.kind() == dc24h::database::ErrorKind::constraint); }
    allocation_failures(options);
}
void read_only(const Options& options) {
    Connection c(options); IdentityRepository repository(c); AccountRepository accounts(c);
    require(repository.find_by_nickname("Alice").has_value());
    const auto target = accounts.find_by_name(*AccountName::create("MappingReadOnlyTarget"));
    require(target.has_value());
    try { (void)repository.bind(target->account.id(),key("DeniedNick",'H'),0); require(false); }
    catch (const dc24h::database::Error& error) { require(!error.outcome_unknown()); }
    require(!c.in_transaction() && !repository.find_by_account(target->account.id()));
}
}
int main(int argc, char* argv[]) {
    try {
        if (argc == 1) { unit(); }
        else if (argc == 3 || argc == 4) {
            Options options; options.enabled = true; options.socket = argv[1]; options.password_file = argv[2];
            if (argc == 4) { options.user = "runtime"; read_only(options); }
            else { integration(options); }
        } else { throw std::runtime_error("Invalid identity mapping test arguments"); }
        std::cout << "Account identity mapping checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
