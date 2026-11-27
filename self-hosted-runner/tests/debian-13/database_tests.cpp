#include "dc24h/database/database.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <source_location>
#include <thread>

namespace {
void require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) { throw std::runtime_error("Database assertion failed at line " + std::to_string(location.line())); }
}
void rejects(const std::function<void()>& action, std::source_location location = std::source_location::current()) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, location);
}
void unit() {
    using namespace dc24h::database;
    Options options;
    validate(options);
    options.enabled = true;
    rejects([&] { validate(options); });
    options.password_file = "private.password";
    validate(options);
    options.socket.clear();
    rejects([&] { validate(options); });
    options.socket = "/tmp/db.sock";
    for (const auto value : {"", "db-name", "db`name", "db;DROP", "db/name"}) {
        options.name = value; rejects([&] { validate(options); });
    }
    options.name = "dc24h";
    for (const auto value : {0U, 61U}) { options.timeout = value; rejects([&] { validate(options); }); }
    options.timeout = 60; validate(options);
    char path[] = "/tmp/dc24h-db-unit-XXXXXX";
    require(::mkdtemp(path) != nullptr);
    const auto dir = std::filesystem::path(path);
    struct Cleanup { std::filesystem::path dir; ~Cleanup() { std::filesystem::remove_all(dir); } } cleanup{dir};
    const auto password = dir / "password";
    const auto write = [&](const std::string& content) {
        std::ofstream stream(password, std::ios::binary); stream << content; stream.close();
        require(::chmod(password.c_str(), 0600) == 0);
    };
    write("example secret'\\\r\n");
    require(read_password(password.string()) == "example secret'\\");
    for (const auto& value : {std::string(), std::string("x\0y", 3), std::string("x\ny"), std::string(4097, 'x')}) {
        write(value); rejects([&] { (void)read_password(password.string()); });
    }
    write(std::string(4096, 'x')); require(read_password(password.string()).size() == 4096);
    require(::chmod(password.c_str(), 0640) == 0);
    rejects([&] { (void)read_password(password.string()); });
    require(::chmod(password.c_str(), 0400) == 0);
    require(read_password(password.string()).size() == 4096);
    const auto link = dir / "link";
    std::filesystem::create_symlink(password, link);
    rejects([&] { (void)read_password(link.string()); });
    std::filesystem::remove(link); std::filesystem::create_hard_link(password, link);
    rejects([&] { (void)read_password(password.string()); });
    rejects([&] { (void)read_password(dir.string()); });
    const auto fifo = dir / "fifo"; require(::mkfifo(fifo.c_str(), 0600) == 0);
    rejects([&] { (void)read_password(fifo.string()); });
}
void integration(char* argv[]) {
    using namespace dc24h::database;
    Options options;
    options.enabled = true; options.socket = argv[1]; options.password_file = argv[2];
    Connection connection(options);
    connection.ping();
    const auto dir = std::filesystem::path(argv[2]).parent_path();
    const auto early = dir / "backup-before-schema.bk";
    rejects([&] { (void)backup(connection, early.string()); });
    require(!std::filesystem::exists(early));
    const std::string hostile = "zażółć' OR 1=1; -- \\";
    require(connection.scalar("SELECT ?", {hostile}) == hostile);
    require(connection.scalar("SELECT NULL") == std::nullopt);
    require(connection.scalar("SELECT REPEAT('a', 4096)")->size() == 4096);
    rejects([&] { (void)connection.scalar("SELECT REPEAT('a', 4097)"); });
    rejects([&] { (void)connection.scalar("SELECT 1, 2"); });
    rejects([&] { (void)connection.scalar("SELECT 1 WHERE FALSE"); });
    rejects([&] { (void)connection.scalar("SELECT 1 UNION ALL SELECT 2"); });
    rejects([&] { (void)connection.scalar("SELECT ?"); });
    rejects([&] { connection.execute("INVALID SQL WITH PRIVATE TOKEN"); });
    require(connection.scalar("SELECT 42") == "42");
    // Closing failed/oversized results leaves the same connection usable.
    Connection rival(options);
    const std::string lock_sql = "CONCAT('dc24h:', LEFT(SHA2(DATABASE(), 256), 52))";
    require(rival.scalar("SELECT GET_LOCK(" + lock_sql + ", 0)") == "1");
    rejects([&] { (void)migrate(connection); });
    require(rival.scalar("SELECT RELEASE_LOCK(" + lock_sql + ")") == "1");
    require(migrate(connection) == migration_catalog().size());
    require(migrate(connection) == 0);
    connection.execute("UPDATE dc24h_schema_migrations SET checksum = ? WHERE version=1", {std::string(64, '0')});
    rejects([&] { (void)migrate(connection); });
    connection.execute("UPDATE dc24h_schema_migrations SET checksum = ? WHERE version=1", {std::string(migration_catalog()[0].checksum)});
    require(migrate(connection) == 0);
    connection.execute("UPDATE dc24h_schema_migrations SET version = 99 WHERE version=2");
    rejects([&] { (void)migrate(connection); });
    connection.execute("UPDATE dc24h_schema_migrations SET version = 2 WHERE version=99");
    require(migrate(connection) == 0);
    const auto locked = dir / "backup-locked.bk";
    require(rival.scalar("SELECT GET_LOCK(" + lock_sql + ", 0)") == "1");
    rejects([&] { (void)backup(connection, locked.string()); });
    require(!std::filesystem::exists(locked));
    require(rival.scalar("SELECT RELEASE_LOCK(" + lock_sql + ")") == "1");
    {
        auto open = connection.transaction();
        const auto blocked = dir / "backup-open.bk";
        rejects([&] { (void)backup(connection, blocked.string()); });
        require(!std::filesystem::exists(blocked));
        open.rollback();
    }
    std::exception_ptr thread_error;
    std::thread worker([&] {
        try {
            (void)backup(connection, (dir / "backup-thread.bk").string());
            thread_error = std::make_exception_ptr(std::runtime_error("wrong thread accepted"));
        } catch (const Error& error) {
            if (error.kind() != ErrorKind::wrong_thread) { thread_error = std::current_exception(); }
        } catch (...) { thread_error = std::current_exception(); }
    });
    worker.join();
    if (thread_error) { std::rethrow_exception(thread_error); }
    require(!std::filesystem::exists(dir / "backup-thread.bk"));
    const Bytes account_name{'b','a','c','k','u','p','-','u','s','e','r'};
    const Bytes verifier{0x00, 0xff, 0x61};
    require(connection.execute_prepared(
        "INSERT INTO dc24h_accounts (id,name,role,status,created_at,updated_at,revision) VALUES (?,?,?,?,?,?,?)",
        {std::int64_t{7}, account_name, Bytes{'r','e','g','i','s','t','e','r','e','d'}, Bytes{'a','c','t','i','v','e'},
         std::int64_t{1}, std::int64_t{1}, std::int64_t{1}}).affected_rows == 1);
    require(connection.execute_prepared(
        "INSERT INTO dc24h_account_credentials (account_id,verifier,created_at) VALUES (?,?,?)",
        {std::int64_t{7}, verifier, std::int64_t{1}}).affected_rows == 1);
    const auto snap = dir / "backup-roundtrip.bk";
    const auto report = backup(connection, snap.string());
    require(report.rows == migration_catalog().size() + 2 && report.bytes > 32 &&
            report.bytes == std::filesystem::file_size(snap));
    struct stat state{};
    require(::stat(snap.c_str(), &state) == 0 && (state.st_mode & 0777) == 0600 && state.st_nlink == 1);
    const auto body = [&] {
        std::ifstream stream(snap, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }();
    const auto secret = read_password(argv[2]);
    require(body.find(secret) == std::string::npos);
    require(body.find(std::string(account_name.begin(), account_name.end())) != std::string::npos);
    require(body.find(std::string(verifier.begin(), verifier.end())) != std::string::npos);
    require(connection.scalar("SELECT IS_FREE_LOCK(" + lock_sql + ")") == "1");
    require(!connection.in_transaction());
    const auto duplicate = dir / "backup-existing.bk";
    std::filesystem::copy_file(snap, duplicate);
    const auto preserved = body;
    rejects([&] { (void)backup(connection, duplicate.string()); });
    require(std::filesystem::file_size(duplicate) == preserved.size());
    const auto link = dir / "backup-link.bk";
    std::filesystem::create_symlink(snap, link);
    rejects([&] { (void)backup(connection, link.string()); });
    require(std::filesystem::file_size(snap) == preserved.size());
    connection.execute("DELETE FROM dc24h_account_credentials");
    connection.execute("DELETE FROM dc24h_accounts");
    auto damaged = preserved;
    damaged.back() = static_cast<char>(static_cast<unsigned char>(damaged.back()) ^ 0x01);
    const auto bad = dir / "backup-damaged.bk";
    { std::ofstream stream(bad, std::ios::binary); stream << damaged; }
    require(::chmod(bad.c_str(), 0600) == 0);
    rejects([&] { (void)restore(connection, bad.string()); });
    require(connection.scalar("SELECT COUNT(*) FROM dc24h_accounts") == "0");
    require(::chmod(snap.c_str(), 0640) == 0);
    rejects([&] { (void)restore(connection, snap.string()); });
    require(::chmod(snap.c_str(), 0600) == 0);
    const auto restored = restore(connection, snap.string());
    require(restored.rows == 2);
    const auto stored = connection.query("SELECT verifier FROM dc24h_account_credentials WHERE account_id=7");
    require(stored.rows.size() == 1 && stored.rows[0][0] && *stored.rows[0][0] == std::string(verifier.begin(), verifier.end()));
    rejects([&] { (void)restore(connection, snap.string()); });
    require(connection.scalar("SELECT COUNT(*) FROM dc24h_accounts") == "1");
    const auto next = connection.execute_prepared(
        "INSERT INTO dc24h_accounts (name,role,status,created_at,updated_at) VALUES (?,?,?,?,?)",
        {Bytes{'n','e','x','t'}, Bytes{'r','e','g','i','s','t','e','r','e','d'}, Bytes{'d','i','s','a','b','l','e','d'}, std::int64_t{2}, std::int64_t{2}});
    require(next.insert_id == 8 && next.affected_rows == 1);
    connection.execute("DELETE FROM dc24h_account_credentials");
    connection.execute("DELETE FROM dc24h_accounts");
    require(connection.scalar("SELECT COUNT(*) FROM dc24h_accounts") == "0");
    require(!connection.in_transaction());
    connection.ping();
    std::filesystem::remove(snap);
    std::filesystem::remove(duplicate);
    std::filesystem::remove(link);
    std::filesystem::remove(bad);

}
}
int main(int argc, char* argv[]) {
    try {
        if (argc == 1) { unit(); }
        else if (argc == 3) { integration(argv); }
        else { throw std::runtime_error("Invalid test arguments"); }
        std::cout << "Database checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
