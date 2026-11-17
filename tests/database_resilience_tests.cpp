#include "dc24h/database/database.hpp"

#include <sys/stat.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <new>
#include <source_location>
#include <thread>

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
using namespace dc24h::database;
using Clock = std::chrono::steady_clock;
void require(bool ok, std::source_location where = std::source_location::current()) {
    if (!ok) { throw std::runtime_error("Database Resilience assertion failed at line " + std::to_string(where.line())); }
}
template<class F> Error rejects(ErrorKind kind, F&& function, std::source_location where = std::source_location::current()) {
    try { function(); }
    catch (const Error& error) { require(error.kind() == kind, where); return error; }
    throw std::runtime_error("Database Resilience accepted invalid operation at line " + std::to_string(where.line()));
}
void timing(Clock::time_point start) {
    const auto elapsed = Clock::now() - start;
    require(elapsed >= std::chrono::milliseconds(500) && elapsed < std::chrono::seconds(8));
}
void unit() {
    Options options;
    validate(options);
    for (auto member : {&Options::connect_timeout, &Options::read_timeout, &Options::write_timeout}) {
        options.*member = 60; validate(options);
        options.*member = 61;
        bool rejected = false;
        try { validate(options); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected); options.*member = 0;
    }
}
void recover(const Options& options) {
    Connection observer(options), c(options);
    c.execute("CREATE TABLE dc24h_resilience_fixture(id INT PRIMARY KEY, value INT NOT NULL) ENGINE=InnoDB");
    c.execute("INSERT INTO dc24h_resilience_fixture VALUES (1,0)");
    c.execute("SET @private_session = 42");
    c.execute("CREATE TEMPORARY TABLE dc24h_resilience_temporary(id INT)");
    require(c.scalar("SELECT GET_LOCK('dc24h_resilience_private',0)") == "1");
    const auto original = c.scalar("SELECT CONNECTION_ID()");
    auto completed = c.transaction(); completed.commit();
    c.reconnect();
    require(c.scalar("SELECT CONNECTION_ID()") != original);
    require(c.scalar("SELECT @private_session") == std::nullopt);
    require(observer.scalar("SELECT IS_FREE_LOCK('dc24h_resilience_private')") == "1");
    (void)rejects(ErrorKind::statement, [&] { (void)c.query("SELECT * FROM dc24h_resilience_temporary"); });
    require(c.scalar("SELECT @@autocommit") == "1" && c.scalar("SELECT @@character_set_connection") == "utf8mb4");
    {
        auto current = c.transaction();
        (void)rejects(ErrorKind::transaction, [&] { completed.commit(); });
        (void)rejects(ErrorKind::transaction, [&] { completed.rollback(); });
        (void)rejects(ErrorKind::transaction, [&] { c.reconnect(); });
        c.execute("UPDATE dc24h_resilience_fixture SET value=value+1 WHERE id=1");
        (void)rejects(ErrorKind::statement, [&] { (void)c.query("SELECT * FROM MissingPrivateSqlTable"); });
        (void)rejects(ErrorKind::transaction, [&] { c.reconnect(); });
        current.rollback();
    }
    require(observer.scalar("SELECT value FROM dc24h_resilience_fixture") == "0");
    // A transport failure must not silently abandon a still-live scope.
    {
        auto tx = c.transaction();
        c.execute("UPDATE dc24h_resilience_fixture SET value=value+1 WHERE id=1");
        observer.execute("KILL CONNECTION " + *c.scalar("SELECT CONNECTION_ID()"));
        (void)rejects(ErrorKind::connection, [&] { c.ping(); });
        (void)rejects(ErrorKind::transaction, [&] { c.reconnect(); });
    }
    c.reconnect();
    require(observer.scalar("SELECT value FROM dc24h_resilience_fixture") == "0");
    {
        auto tx = c.transaction();
        c.execute("UPDATE dc24h_resilience_fixture SET value=value+1 WHERE id=1");
        observer.execute("KILL CONNECTION " + *c.scalar("SELECT CONNECTION_ID()"));
        const auto error = rejects(ErrorKind::connection, [&] { tx.commit(); });
        require(error.operation() == Operation::commit && error.outcome_unknown());
        c.reconnect(); // Failed commit finished the scope; old weak link expires.
        auto fresh = c.transaction();
        (void)rejects(ErrorKind::transaction, [&] { tx.commit(); });
        c.execute("UPDATE dc24h_resilience_fixture SET value=value+1 WHERE id=1");
        fresh.commit();
    }
    require(observer.scalar("SELECT value FROM dc24h_resilience_fixture") == "1");
    // Closing a killed idle connection is explicit; ordinary operations never retry.
    observer.execute("KILL CONNECTION " + *c.scalar("SELECT CONNECTION_ID()"));
    (void)rejects(ErrorKind::connection, [&] { (void)c.query("SELECT 1"); });
    (void)rejects(ErrorKind::connection, [&] { c.ping(); });
    c.reconnect(); require(c.scalar("SELECT 42") == "42");
    // Wrong-thread reconnect leaves the owner's session untouched.
    const auto key = c.scalar("SELECT CONNECTION_ID()");
    std::exception_ptr failure;
    std::jthread foreign([&] { try { (void)rejects(ErrorKind::wrong_thread, [&] { c.reconnect(); }); }
        catch (...) { failure = std::current_exception(); } });
    foreign.join(); if (failure) { std::rethrow_exception(failure); }
    require(c.scalar("SELECT CONNECTION_ID()") == key);
    {
        auto tx = c.transaction();
        c.execute("UPDATE dc24h_resilience_fixture SET value=value+1 WHERE id=1");
        std::jthread cleanup([scope = std::move(tx)] {}); cleanup.join();
    }
    c.reconnect();
    require(observer.scalar("SELECT value FROM dc24h_resilience_fixture") == "1");
    // Re-read credentials on every attempt; failure retires even a healthy session.
    const auto secret = read_password(options.password_file);
    const auto write = [&](const std::string& value) { std::ofstream file(options.password_file); file << value << '\n'; require(bool(file)); };
    write("IncorrectReplacementPrivatePassword");
    const auto denied = rejects(ErrorKind::connection, [&] { c.reconnect(); });
    require(denied.operation() == Operation::connect && !denied.outcome_unknown());
    (void)rejects(ErrorKind::connection, [&] { c.ping(); });
    write(secret); require(::chmod(options.password_file.c_str(), 0644) == 0);
    (void)rejects(ErrorKind::credentials, [&] { c.reconnect(); });
    require(::chmod(options.password_file.c_str(), 0600) == 0);
    c.reconnect(); require(c.scalar("SELECT 42") == "42");
    bool failed = false, succeeded = false;
    for (int budget = 0; budget < 16; ++budget) {
        fail_after = budget;
        try { c.reconnect(); succeeded = true; }
        catch (const std::bad_alloc&) { failed = true; }
        fail_after = -1;
        c.reconnect(); require(c.scalar("SELECT 42") == "42");
    }
    require(failed && succeeded);
    require(observer.scalar("SELECT value FROM dc24h_resilience_fixture") == "1");
}
void timeouts(Options options) {
    // Prove both the legacy fallback and independent read override on real SQL.
    for (bool override : {false, true}) {
        options.timeout = override ? 10 : 1;
        options.read_timeout = override ? 1 : 0;
        Connection c(options);
        const auto start = Clock::now();
        const auto error = rejects(ErrorKind::connection, [&] { (void)c.query("SELECT SLEEP(12)"); });
        timing(start); require(error.outcome_unknown() && error.native_code() != 0);
        (void)rejects(ErrorKind::connection, [&] { c.ping(); });
        c.reconnect(); require(c.scalar("SELECT 42") == "42");
    }
    options.timeout = 10; options.read_timeout = 1;
    Connection c(options);
    const auto start = Clock::now();
    const auto error = rejects(ErrorKind::connection, [&] { c.execute("DO SLEEP(12)"); });
    timing(start); require(error.operation() == Operation::execute && error.outcome_unknown());
    c.reconnect(); c.ping();
}
void restart(const Options& options) {
    Connection c(options);
    c.execute("CREATE TABLE dc24h_restart_fixture(id INT PRIMARY KEY, value INT) ENGINE=InnoDB");
    c.execute("INSERT INTO dc24h_restart_fixture VALUES (1,1)");
    auto command = [] (std::string_view expected) { std::string line; require(bool(std::getline(std::cin, line)) && line == expected); };
    std::cout << "ready" << std::endl;
    command("down");
    (void)rejects(ErrorKind::connection, [&] { c.ping(); });
    (void)rejects(ErrorKind::connection, [&] { c.reconnect(); });
    (void)rejects(ErrorKind::connection, [&] { (void)c.query("SELECT 1"); });
    std::cout << "down" << std::endl;
    command("up"); c.reconnect();
    require(c.scalar("SELECT value FROM dc24h_restart_fixture") == "1");
    require(c.scalar("SELECT @@autocommit") == "1");
    std::cout << "recovered" << std::endl;
}
void handshake(Options options) {
    options.timeout = 10; options.connect_timeout = 1; options.read_timeout = 10;
    const auto start = Clock::now();
    const auto error = rejects(ErrorKind::connection, [&] { Connection c(options); });
    timing(start); require(error.operation() == Operation::connect && !error.outcome_unknown());
}
void write_stall(Options options) {
    options.timeout = 10; options.write_timeout = 1;
    Connection c(options);
    Parameters parameters(16, std::string(65536, 'x'));
    std::string sql = "DO LENGTH(CONCAT(?";
    for (int n = 1; n < 16; ++n) { sql += ",?"; }
    sql += "))";
    const auto start = Clock::now();
    const auto error = rejects(ErrorKind::connection, [&] { (void)c.execute_prepared(sql, parameters); });
    timing(start); require(error.operation() == Operation::execute && error.outcome_unknown());
    (void)rejects(ErrorKind::connection, [&] { c.ping(); });
}
void commit_ack(const Options& options) {
    Connection c(options);
    c.execute("CREATE TABLE dc24h_ack_fixture(id INT PRIMARY KEY, value INT NOT NULL) ENGINE=InnoDB");
    c.execute("INSERT INTO dc24h_ack_fixture VALUES (1,0)");
    auto tx = c.transaction();
    c.execute("UPDATE dc24h_ack_fixture SET value=value+1 WHERE id=1");
    const auto error = rejects(ErrorKind::connection, [&] { tx.commit(); });
    require(error.operation() == Operation::commit && error.outcome_unknown());
    (void)rejects(ErrorKind::connection, [&] { (void)c.query("SELECT 1"); });
}
}
int main(int argc, char* argv[]) {
    try {
        if (argc == 1) { unit(); }
        else {
            require(argc == 3 || argc == 4);
            Options options; options.enabled = true; options.socket = argv[1]; options.password_file = argv[2]; options.timeout = 2;
            if (argc == 3) { recover(options); timeouts(options); }
            else if (std::string_view(argv[3]) == "restart") { restart(options); }
            else if (std::string_view(argv[3]) == "handshake") { handshake(options); }
            else if (std::string_view(argv[3]) == "write") { write_stall(options); }
            else if (std::string_view(argv[3]) == "commit_ack") { commit_ack(options); }
            else { throw std::runtime_error("Invalid Database Resilience test mode"); }
        }
        std::cout << "Database Resilience checks passed\n";
    } catch (const Error& error) {
        fail_after = -1;
        std::cerr << error.what() << " operation=" << static_cast<unsigned>(error.operation())
                  << " code=" << error.native_code() << '\n'; return 1;
    } catch (const std::exception& error) {
        fail_after = -1; std::cerr << error.what() << '\n'; return 1;
    }
}
