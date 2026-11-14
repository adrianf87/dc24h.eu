#include "dc24h/database/database.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <source_location>

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
    require(migrate(connection) == 2);
    require(migrate(connection) == 0);
    connection.execute("UPDATE dc24h_schema_migrations SET checksum = ? WHERE version=1", {std::string(64, '0')});
    rejects([&] { (void)migrate(connection); });
    connection.execute("UPDATE dc24h_schema_migrations SET checksum = ? WHERE version=1", {std::string(migration_catalog()[0].checksum)});
    require(migrate(connection) == 0);
    connection.execute("UPDATE dc24h_schema_migrations SET version = 99 WHERE version=2");
    rejects([&] { (void)migrate(connection); });
    connection.execute("UPDATE dc24h_schema_migrations SET version = 2 WHERE version=99");
    require(migrate(connection) == 0);

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
