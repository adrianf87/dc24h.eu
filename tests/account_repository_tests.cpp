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
    if (!ok) { throw std::runtime_error("Account repository assertion at line " + std::to_string(where.line())); }
}
template<class F> void rejects(RepositoryErrorKind kind, F&& action,
    std::source_location where = std::source_location::current()) {
    try { action(); }
    catch (const RepositoryError& error) { require(error.kind() == kind,where); return; }
    require(false,where);
}
Account candidate(std::variant<Account,AccountError> value) {
    require(std::holds_alternative<Account>(value));
    return std::get<Account>(std::move(value));
}
void allocation_failures(const Options& options) {
    Connection observer(options);
    int insert_failures = 0, update_failures = 0;
    bool inserted = false, updated = false;
    for (int index = 0; index < 512 && !inserted; ++index) {
        Connection c(options); AccountRepository repository(c);
        const NewAccount input{"AllocationInsert"};
        fail_after = index;
        try { (void)repository.insert(input); fail_after = -1; inserted = true; }
        catch (const std::bad_alloc&) { fail_after = -1; ++insert_failures; }
        require(!c.in_transaction());
        require(observer.scalar("SELECT COUNT(*) FROM dc24h_accounts WHERE name='AllocationInsert'") == (inserted ? "1" : "0"));
    }
    AccountRepository reader(observer);
    const auto original = reader.insert({"AllocationUpdate"});
    const auto next = candidate(original.account.with_name("AllocationChanged",0));
    for (int index = 0; index < 512 && !updated; ++index) {
        Connection c(options); AccountRepository repository(c);
        fail_after = index;
        try { (void)repository.update(original,next); fail_after = -1; updated = true; }
        catch (const std::bad_alloc&) { fail_after = -1; ++update_failures; }
        require(!c.in_transaction());
        const auto found = reader.find_by_id(original.account.id());
        require(found && found->account == (updated ? next : original.account));
        require(found->revision == (updated ? 2U : 1U));
    }
    require(inserted && updated && insert_failures > 30 && update_failures > 30);
}
void integration(const Options& options) {
    Connection c(options);
    AccountRepository repository(c);
    const auto name = *AccountName::create("Zażółć'\\;--");
    require(!repository.find_by_name(name));
    auto first = repository.insert({name.text(),Role::registered,Status::disabled,{0,0,0}});
    require(first.account.valid() && first.revision == 1 && first.account.times().last_login_at == 0);
    require(repository.find_by_id(first.account.id()) == first);
    c.execute("ALTER TABLE dc24h_accounts DROP INDEX dc24h_accounts_name");
    rejects(RepositoryErrorKind::schema,[&] { (void)repository.insert({"Unsafe"}); });
    c.execute("ALTER TABLE dc24h_accounts ADD UNIQUE KEY dc24h_accounts_name(name)");
    c.execute("ALTER TABLE dc24h_account_credentials DROP FOREIGN KEY dc24h_credentials_account");
    c.execute("ALTER TABLE dc24h_account_adc_credentials DROP FOREIGN KEY dc24h_adc_credentials_account");
    c.execute("ALTER TABLE dc24h_account_identities DROP FOREIGN KEY dc24h_identities_account");
    c.execute("ALTER TABLE dc24h_bans DROP FOREIGN KEY dc24h_bans_creator, DROP FOREIGN KEY dc24h_bans_revoker");
    c.execute("ALTER TABLE dc24h_accounts ENGINE=MyISAM");
    rejects(RepositoryErrorKind::schema,[&] { (void)repository.find_by_id(first.account.id()); });
    c.execute("ALTER TABLE dc24h_accounts ENGINE=InnoDB");
    c.execute("ALTER TABLE dc24h_bans ADD CONSTRAINT dc24h_bans_creator FOREIGN KEY(created_by) REFERENCES dc24h_accounts(id) ON DELETE RESTRICT ON UPDATE RESTRICT, ADD CONSTRAINT dc24h_bans_revoker FOREIGN KEY(revoked_by) REFERENCES dc24h_accounts(id) ON DELETE RESTRICT ON UPDATE RESTRICT");
    c.execute("ALTER TABLE dc24h_account_adc_credentials ADD CONSTRAINT dc24h_adc_credentials_account FOREIGN KEY(account_id) REFERENCES dc24h_accounts(id) ON UPDATE RESTRICT ON DELETE RESTRICT");
    c.execute("ALTER TABLE dc24h_account_identities ADD CONSTRAINT dc24h_identities_account FOREIGN KEY(account_id) REFERENCES dc24h_accounts(id) ON DELETE RESTRICT ON UPDATE RESTRICT");
    c.execute("ALTER TABLE dc24h_account_credentials ADD CONSTRAINT dc24h_credentials_account FOREIGN KEY(account_id) REFERENCES dc24h_accounts(id) ON DELETE RESTRICT ON UPDATE RESTRICT");
    require(repository.find_by_name(name) == first);
    // Owning rows survive connection destruction and are visible to new connections.
    { Connection other(options); AccountRepository reader(other);
      require(reader.find_by_id(first.account.id()) == first); }
    auto alice = repository.insert({"Alice"});
    auto lower = repository.insert({"alice"});
    require(alice.account.id() != lower.account.id());
    require(!alice.account.times().last_login_at && alice.account.status() == Status::disabled);
    require(repository.find_by_name(*AccountName::create("Alice")) == alice);
    require(repository.find_by_name(*AccountName::create("alice")) == lower);
    require(!repository.find_by_name(*AccountName::create("ALICE")));
    rejects(RepositoryErrorKind::duplicate,[&] { (void)repository.insert({"Alice"}); });
    require(!c.in_transaction());
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.insert({"bad name"}); });
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.insert({"bad",Role::registered,Status::active,{4,3,{}}}); });
    auto moved = *AccountName::create("Moved"); auto owner = std::move(moved);
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.find_by_name(moved); });
    (void)owner;
    // Revision prevents lost updates even when both edits have the same UTC second.
    auto renamed = candidate(alice.account.with_name("Renamed",0));
    auto saved = repository.update(alice,renamed);
    require(saved.revision == 2 && !repository.find_by_name(*AccountName::create("Alice")));
    rejects(RepositoryErrorKind::stale,[&] { (void)repository.update(alice,renamed); });
    auto no_op = repository.update(saved,saved.account);
    require(no_op.revision == 3);
    rejects(RepositoryErrorKind::duplicate,[&] {
        (void)repository.update(no_op,candidate(no_op.account.with_name("alice",0)));
    });
    require(repository.find_by_id(no_op.account.id()) == no_op && !c.in_transaction());
    auto logged = repository.update(no_op,candidate(no_op.account.with_last_login(10)));
    auto access = candidate(logged.account.with_access(Role::administrator,Status::active,10));
    logged = repository.update(logged,access);
    auto data = logged.account.snapshot(); data.times.created_at = 1;
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.update(logged,candidate(Account::create(data))); });
    data = logged.account.snapshot(); data.times.last_login_at.reset();
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.update(logged,candidate(Account::create(data))); });
    data = logged.account.snapshot(); data.times = {0,9,9};
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.update(logged,candidate(Account::create(data))); });
    auto fake = logged; fake.revision = 0;
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.update(fake,access); });
    fake = logged; fake.revision = 9223372036854775807ULL;
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.update(fake,access); });
    fake = logged; fake.account = candidate(fake.account.with_name("Forged",10));
    rejects(RepositoryErrorKind::stale,[&] { (void)repository.update(fake,access); });
    { auto transaction = c.transaction();
      try { (void)repository.find_by_id(first.account.id()); require(false); }
      catch (const dc24h::database::Error& error) { require(error.kind() == dc24h::database::ErrorKind::transaction); }
      transaction.rollback(); }
    auto boundary = repository.insert({std::string(64,'x'),Role::moderator,Status::active,{max_timestamp,max_timestamp,max_timestamp}});
    require(repository.find_by_id(boundary.account.id()) == boundary);
    rejects(RepositoryErrorKind::invalid_account,[&] { (void)repository.insert({std::string(65,'x')}); });

    // Two independent sessions race on one expected revision: exactly one wins.
    std::barrier gate(2);
    int wins[2]{}, stale[2]{}; std::exception_ptr errors[2];
    auto race = [&](int index) {
        try {
            Connection db(options); AccountRepository writer(db);
            const auto next = candidate(lower.account.with_name(index ? "RaceB" : "RaceA",1));
            gate.arrive_and_wait();
            try { (void)writer.update(lower,next); wins[index] = 1; }
            catch (const RepositoryError& error) { require(error.kind() == RepositoryErrorKind::stale); stale[index] = 1; }
        } catch (...) { errors[index] = std::current_exception(); gate.arrive_and_drop(); }
    };
    std::jthread one(race,0), two(race,1); one.join(); two.join();
    for (auto error : errors) { if (error) { std::rethrow_exception(error); } }
    require(wins[0]+wins[1] == 1 && stale[0]+stale[1] == 1);

    // Imported/corrupt records fail closed, without exposing stored names.
    c.execute("SET check_constraint_checks=OFF");
    c.execute("UPDATE dc24h_accounts SET role='InvalidRole' WHERE id=?",{std::to_string(first.account.id().value())});
    try { (void)repository.find_by_id(first.account.id()); require(false); }
    catch (const RepositoryError& error) {
        require(error.kind() == RepositoryErrorKind::invalid_record);
        require(std::string_view(error.what()).find("InvalidRole") == std::string_view::npos);
    }
    c.execute("UPDATE dc24h_accounts SET role='registered',name=? WHERE id=?",{std::string("bad\0name",8),std::to_string(first.account.id().value())});
    rejects(RepositoryErrorKind::invalid_record,[&] { (void)repository.find_by_id(first.account.id()); });
    c.execute("UPDATE dc24h_accounts SET name=?,revision=0 WHERE id=?",{name.text(),std::to_string(first.account.id().value())});
    rejects(RepositoryErrorKind::invalid_record,[&] { (void)repository.find_by_id(first.account.id()); });
    c.execute("UPDATE dc24h_accounts SET revision=1 WHERE id=?",{std::to_string(first.account.id().value())});
    c.execute("SET check_constraint_checks=ON");
    require(repository.find_by_id(first.account.id()) == first);
    c.execute("UPDATE dc24h_schema_migrations SET state='applying' WHERE version=8");
    rejects(RepositoryErrorKind::schema,[&] { (void)repository.find_by_id(first.account.id()); });
    c.execute("UPDATE dc24h_schema_migrations SET state='applied' WHERE version=8");
    c.execute("DELETE FROM dc24h_accounts WHERE id=?",{std::to_string(first.account.id().value())});
    require(!repository.find_by_id(first.account.id()));
    rejects(RepositoryErrorKind::stale,[&] { (void)repository.update(first,first.account); });
    // Leave a known row for a second executable process and the read-only user.
    (void)repository.insert({"Persistent"});
    allocation_failures(options);
}
void read_only(const Options& options) {
    Connection c(options); AccountRepository repository(c);
    const auto row = repository.find_by_name(*AccountName::create("Persistent")); require(row.has_value());
    for (int action = 0; action < 2; ++action) {
        try {
            if (action == 0) { (void)repository.insert({"Denied"}); }
            else { (void)repository.update(*row,candidate(row->account.with_name("Denied",0))); }
            require(false);
        } catch (const dc24h::database::Error& error) { require(!error.outcome_unknown()); }
        require(!c.in_transaction() && repository.find_by_id(row->account.id()) == row);
    }
}
}
int main(int argc, char* argv[]) {
    try {
        if (argc != 3 && argc != 4) { throw std::runtime_error("Invalid account test arguments"); }
        Options options; options.enabled = true; options.socket = argv[1]; options.password_file = argv[2];
        if (argc == 4) { options.user = "runtime"; read_only(options); }
        else { integration(options); }
        std::cout << "Account repository checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
