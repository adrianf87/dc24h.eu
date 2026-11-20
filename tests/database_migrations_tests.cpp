#include "migrations_internal.hpp"
#include "dc24h/database/migrations.hpp"
#include <openssl/sha.h>

#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <source_location>
#include <thread>

namespace {
using namespace dc24h::database;
using detail::Entry;
void require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) { throw std::runtime_error("Migration assertion failed at line " + std::to_string(location.line())); }
}
template<class F> void rejects(MigrationErrorKind kind, F&& f, std::source_location location = std::source_location::current()) {
    try { f(); } catch (const MigrationError& e) { require(e.kind() == kind,location); return; }
    require(false,location);
}
std::string checksum(std::span<const std::string_view> sql) {
    std::string input, result;
    for (const auto text : sql) { if (sql.size() > 1) { input += std::to_string(text.size()) + ':'; } input += text; }
    std::array<unsigned char,SHA256_DIGEST_LENGTH> bytes{};
    SHA256(reinterpret_cast<const unsigned char*>(input.data()),input.size(),bytes.data());
    constexpr char hex[] = "0123456789abcdef";
    for (const auto b : bytes) { result += hex[b >> 4]; result += hex[b & 15]; }
    return result;
}
void unit() {
    const auto& entries = detail::builtin_migrations;
    detail::validate_catalog(entries);
    require(migration_catalog().size() == 10 && entries[0].info.version == 1 && entries[1].info.version == 2);
    require(entries[8].info.version == 9 && entries[8].info.name == "audit_events");
    require(entries[9].info.version == 10 && entries[9].info.name == "configuration_audit");
    require(entries[0].info.name == "schema_migrations" && entries[1].info.name == "migration_state");
    auto invalid = entries;
    invalid[1].info.version = 3;
    rejects(MigrationErrorKind::catalog,[&] { detail::validate_catalog(invalid); });
    invalid = entries; invalid[0].info.checksum = std::string_view("wrong");
    rejects(MigrationErrorKind::catalog,[&] { detail::validate_catalog(invalid); });
    invalid = entries; invalid[1].info.name = "BadPrivateName";
    rejects(MigrationErrorKind::catalog,[&] { detail::validate_catalog(invalid); });
    invalid = entries; invalid[0].statements = {};
    rejects(MigrationErrorKind::catalog,[&] { detail::validate_catalog(invalid); });
    rejects(MigrationErrorKind::catalog,[&] { detail::validate_catalog({}); });
    constexpr std::array<std::string_view,1> nul{std::string_view("x\0y",3)};
    invalid = entries; invalid[1].statements = nul;
    rejects(MigrationErrorKind::catalog,[&] { detail::validate_catalog(invalid); });
}
void legacy(Connection& c) {
    const auto catalog = migration_catalog();
    const auto bootstrap = std::span(detail::builtin_migrations).first(2);
    require(detail::inspect(c,bootstrap).current_version == 2 && detail::run(c,bootstrap) == 0);
    const auto original_time = c.scalar("SELECT applied_at FROM dc24h_schema_migrations WHERE version=1");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=2");
    c.execute("ALTER TABLE dc24h_schema_migrations DROP COLUMN state");
    auto status = detail::inspect(c,bootstrap);
    require(status.current_version == 1 && status.latest_version == 2 && status.pending == 1 && !status.dirty);
    c.execute("UPDATE dc24h_schema_migrations SET checksum=REPEAT('0',64) WHERE version=1");
    rejects(MigrationErrorKind::history,[&] { (void)detail::run(c,bootstrap); });
    require(c.scalar("SELECT COUNT(*) FROM information_schema.columns WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_schema_migrations' AND COLUMN_NAME='state'") == "0");
    c.execute("UPDATE dc24h_schema_migrations SET checksum=? WHERE version=1",{std::string(catalog[0].checksum)});
    require(detail::run(c,bootstrap) == 1 && detail::run(c,bootstrap) == 0);
    require(c.scalar("SELECT applied_at FROM dc24h_schema_migrations WHERE version=1") == original_time);
    // Recover the safe v2 bootstrap gap: ALTER succeeded, metadata insert did not.
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=2");
    require(detail::inspect(c,bootstrap).pending == 1 && detail::run(c,bootstrap) == 1);
    c.execute("UPDATE dc24h_schema_migrations SET name='PrivateMismatch' WHERE version=2");
    rejects(MigrationErrorKind::history,[&] { (void)detail::inspect(c,bootstrap); });
    c.execute("UPDATE dc24h_schema_migrations SET name='migration_state' WHERE version=2");
    c.execute("UPDATE dc24h_schema_migrations SET state='applying' WHERE version=2");
    rejects(MigrationErrorKind::history,[&] { (void)detail::run(c,bootstrap); });
    c.execute("UPDATE dc24h_schema_migrations SET state='applied' WHERE version=2");
    c.execute("UPDATE dc24h_schema_migrations SET version=99 WHERE version=2");
    rejects(MigrationErrorKind::history,[&] { (void)detail::run(c,bootstrap); });
    c.execute("UPDATE dc24h_schema_migrations SET version=2 WHERE version=99");
    c.execute("ALTER TABLE dc24h_schema_migrations MODIFY state VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NULL DEFAULT 'applied'");
    rejects(MigrationErrorKind::schema,[&] { (void)detail::run(c,bootstrap); });
    c.execute("ALTER TABLE dc24h_schema_migrations MODIFY state VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'applied'");
    require(detail::run(c,bootstrap) == 0);
}
void locking(Connection& c, const Options& options) {
    const std::string expression = "CONCAT('dc24h:',LEFT(SHA2(DATABASE(),256),52))";
    Connection rival(options);
    require(rival.scalar("SELECT GET_LOCK("+expression+",0)") == "1");
    rejects(MigrationErrorKind::lock_unavailable,[&] { (void)migrate(c); });
    require(inspect_migrations(c).current_version == 2); // inspection takes no lock
    require(rival.scalar("SELECT RELEASE_LOCK("+expression+")") == "1");
    require(c.scalar("SELECT GET_LOCK("+expression+",0)") == "1");
    rejects(MigrationErrorKind::lock_unavailable,[&] { (void)migrate(c); });
    require(c.scalar("SELECT RELEASE_LOCK("+expression+")") == "1");
    require(rival.scalar("SELECT GET_LOCK("+expression+",0)") == "1"); // no recursive lock leaked
    require(rival.scalar("SELECT RELEASE_LOCK("+expression+")") == "1");
    { auto tx = c.transaction();
      try { (void)inspect_migrations(c); require(false); } catch (const Error& e) { require(e.kind() == ErrorKind::transaction); }
      try { (void)migrate(c); require(false); } catch (const Error& e) { require(e.kind() == ErrorKind::transaction); }
      tx.rollback(); }
}
void future(Connection& c, const Options& options) {
    constexpr std::array<std::string_view,2> third{
        "CREATE TABLE dc24h_migration_fixture(value VARCHAR(64)) ENGINE=InnoDB",
        "INSERT INTO dc24h_migration_fixture VALUES ('Unicode ż; -- quote \'\' safe')"};
    constexpr std::array<std::string_view,2> fourth{
        "CREATE TABLE dc24h_partial_fixture(id INT) ENGINE=InnoDB",
        "INVALID SQL WITH PrivateMigrationToken"};
    const auto hash3 = checksum(third), hash4 = checksum(fourth);
    const std::array<Entry,4> entries{detail::builtin_migrations[0],detail::builtin_migrations[1],
        Entry{{3,"test_fixture",hash3},third},Entry{{4,"test_partial",hash4},fourth}};
    detail::validate_catalog(entries);
    require(detail::run(c,std::span(entries).first(3)) == 1);
    require(c.scalar("SELECT value FROM dc24h_migration_fixture") == "Unicode ż; -- quote ' safe");
    require(detail::run(c,std::span(entries).first(3)) == 0);
    rejects(MigrationErrorKind::history,[&] { (void)migrate(c); }); // older catalog refuses downgrade
    try { (void)detail::run(c,entries); require(false); }
    catch (const Error& e) { require(std::string_view(e.what()).find("PrivateMigrationToken") == std::string_view::npos); }
    const auto status = detail::inspect(c,entries);
    require(status.dirty && status.current_version == 3 && status.pending == 1);
    require(c.scalar("SELECT COUNT(*) FROM dc24h_partial_fixture") == "0");
    rejects(MigrationErrorKind::dirty,[&] { (void)detail::run(c,entries); });
    require(c.scalar("SELECT state FROM dc24h_schema_migrations WHERE version=4") == "applying");
    // Only fixture teardown reverses effects/deletes journal rows; the product
    // exposes no repair, mark-applied, down or retry-dirty action.
    c.execute("DROP TABLE dc24h_partial_fixture"); c.execute("DROP TABLE dc24h_migration_fixture");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version>=3");

    // Kill a real runner after durable intent and DDL, during its next step.
    constexpr std::array<std::string_view,2> interrupted{
        "CREATE TABLE dc24h_interrupted_fixture(id INT) ENGINE=InnoDB", "DO SLEEP(10)"};
    const auto hash = checksum(interrupted);
    const std::array<Entry,3> crash{entries[0],entries[1],Entry{{3,"test_interrupted",hash},interrupted}};
    Connection victim(options);
    const auto id = *victim.scalar("SELECT CONNECTION_ID()");
    std::exception_ptr error;
    std::jthread killer([&] {
        try {
            Connection control(options);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (control.scalar("SELECT COUNT(*) FROM information_schema.tables WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_interrupted_fixture'") != "1") {
                require(std::chrono::steady_clock::now() < deadline);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            require(control.scalar("SELECT state FROM dc24h_schema_migrations WHERE version=3") == "applying");
            control.execute("KILL CONNECTION " + id);
        } catch (...) { error = std::current_exception(); }
    });
    try { (void)detail::run(victim,crash); require(false); }
    catch (const Error& e) { require(e.kind() == ErrorKind::connection); }
    killer.join(); if (error) { std::rethrow_exception(error); }
    require(detail::inspect(c,crash).dirty);
    rejects(MigrationErrorKind::dirty,[&] { (void)detail::run(c,crash); });
    c.execute("DROP TABLE dc24h_interrupted_fixture");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=3");
    require(migrate(c) == 8); // Restore accounts, credentials, mappings, ADC secrets, roles, bans and audit v9/v10.
}
void adc_upgrade(Connection& c) {
    c.execute("INSERT INTO dc24h_accounts(name,role,status,created_at,updated_at) VALUES ('AdcMigrationSentinel','registered','disabled',1,1)");
    c.execute("INSERT INTO dc24h_account_credentials(account_id,verifier,created_at) SELECT id,'MigrationVerifierSentinel',1 FROM dc24h_accounts WHERE name='AdcMigrationSentinel'");
    const auto before = c.query("SELECT account_id,verifier,created_at FROM dc24h_account_credentials ORDER BY account_id");
    const auto applied = c.scalar("SELECT applied_at FROM dc24h_schema_migrations WHERE version=5");
    c.execute("DROP TABLE dc24h_account_adc_credentials");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=6");
    const auto status = detail::inspect(c,std::span(detail::builtin_migrations).first(6));
    require(status.current_version == 5 && status.latest_version == 6 && status.pending == 1 && !status.dirty);
    require(detail::run(c,std::span(detail::builtin_migrations).first(6)) == 1 && detail::run(c,std::span(detail::builtin_migrations).first(6)) == 0);
    require(c.query("SELECT account_id,verifier,created_at FROM dc24h_account_credentials ORDER BY account_id").rows == before.rows);
    require(c.scalar("SELECT applied_at FROM dc24h_schema_migrations WHERE version=5") == applied);
    require(c.scalar("SELECT COUNT(*) FROM dc24h_account_adc_credentials") == "0");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=6");
    try { (void)detail::run(c,std::span(detail::builtin_migrations).first(6)); require(false); } catch (const Error&) {}
    require(detail::inspect(c,std::span(detail::builtin_migrations).first(6)).dirty);
    rejects(MigrationErrorKind::dirty,[&] { (void)detail::run(c,std::span(detail::builtin_migrations).first(6)); });
    c.execute("DROP TABLE dc24h_account_adc_credentials");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=6");
    require(detail::run(c,std::span(detail::builtin_migrations).first(6)) == 1);
}
void identity_upgrade(Connection& c) {
    c.execute("DROP TABLE dc24h_account_adc_credentials");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=6");
    const auto catalog = std::span(detail::builtin_migrations).first(5);
    c.execute("INSERT INTO dc24h_accounts(name,role,status,created_at,updated_at) VALUES ('IdentityMigrationSentinel','registered','disabled',0,0)");
    const auto before = c.query("SELECT id,name,role,status,created_at,updated_at,last_login_at,revision FROM dc24h_accounts ORDER BY id");
    const auto applied = c.scalar("SELECT applied_at FROM dc24h_schema_migrations WHERE version=4");
    c.execute("DROP TABLE dc24h_account_identities");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=5");
    const auto status = detail::inspect(c,catalog);
    require(status.current_version == 4 && status.latest_version == 5 && status.pending == 1 && !status.dirty);
    require(detail::run(c,catalog) == 1 && detail::run(c,catalog) == 0);
    require(c.query("SELECT id,name,role,status,created_at,updated_at,last_login_at,revision FROM dc24h_accounts ORDER BY id").rows == before.rows);
    require(c.scalar("SELECT applied_at FROM dc24h_schema_migrations WHERE version=4") == applied);
    require(c.scalar("SELECT COUNT(*) FROM dc24h_account_identities") == "0");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=5");
    try { (void)detail::run(c,catalog); require(false); } catch (const Error&) {}
    require(detail::inspect(c,catalog).dirty);
    rejects(MigrationErrorKind::dirty,[&] { (void)detail::run(c,catalog); });
    c.execute("DROP TABLE dc24h_account_identities");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=5");
    require(detail::run(c,catalog) == 1);
    require(detail::run(c,std::span(detail::builtin_migrations).first(6)) == 1); // Restore schema v6.
}
void credential_upgrade(Connection& c) {
    c.execute("DROP TABLE dc24h_account_adc_credentials");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=6");
    const auto catalog = std::span(detail::builtin_migrations).first(4);
    // Preserve the historical v3-to-v4 fixture against its own exact catalog.
    c.execute("DROP TABLE dc24h_account_identities");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=5");
    c.execute("INSERT INTO dc24h_accounts(name,role,status,created_at,updated_at,last_login_at) VALUES ('MigrationSentinel','moderator','disabled',7,8,7)");
    const auto before = c.query("SELECT id,name,role,status,created_at,updated_at,last_login_at,revision FROM dc24h_accounts WHERE name='MigrationSentinel'");
    const auto applied = c.scalar("SELECT applied_at FROM dc24h_schema_migrations WHERE version=3");
    c.execute("DROP TABLE dc24h_account_credentials");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=4");
    const auto status = detail::inspect(c,catalog);
    require(status.current_version == 3 && status.latest_version == 4 && status.pending == 1 && !status.dirty);
    require(detail::run(c,catalog) == 1 && detail::run(c,catalog) == 0);
    require(c.query("SELECT id,name,role,status,created_at,updated_at,last_login_at,revision FROM dc24h_accounts WHERE name='MigrationSentinel'").rows == before.rows);
    require(c.scalar("SELECT applied_at FROM dc24h_schema_migrations WHERE version=3") == applied);
    require(c.scalar("SELECT COUNT(*) FROM dc24h_account_credentials") == "0");
    // Actual shipped v4 refuses to guess whether an existing table is safe.
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=4");
    try { (void)detail::run(c,catalog); require(false); } catch (const Error&) {}
    require(detail::inspect(c,catalog).dirty);
    rejects(MigrationErrorKind::dirty,[&] { (void)detail::run(c,catalog); });
    // Explicit fixture teardown, not a product repair/replay operation.
    c.execute("DROP TABLE dc24h_account_credentials");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=4");
    require(detail::run(c,catalog) == 1);
    require(detail::run(c,std::span(detail::builtin_migrations).first(6)) == 2); // Restore mappings and ADC credentials.
}
void integration(const Options& options) {
    Connection c(options);
    // Upgrade v8 -> v10 without changing account or ban rows.
    const auto before_accounts=c.query("SELECT id,name,role,status,revision FROM dc24h_accounts ORDER BY id");
    const auto before_ban_rows=c.query("SELECT id,kind,subject,reason,created_at,expires_at,created_by,revoked_at,revoked_by,revision FROM dc24h_bans ORDER BY id");
    c.execute("DROP TABLE dc24h_audit_events");c.execute("DELETE FROM dc24h_schema_migrations WHERE version>=9");
    require(migrate(c)==2&&migrate(c)==0);
    require(c.query("SELECT id,name,role,status,revision FROM dc24h_accounts ORDER BY id").rows==before_accounts.rows);
    require(c.query("SELECT id,kind,subject,reason,created_at,expires_at,created_by,revoked_at,revoked_by,revision FROM dc24h_bans ORDER BY id").rows==before_ban_rows.rows);
    require(c.scalar("SELECT COUNT(*) FROM dc24h_audit_events")=="0");
    // Return to the v7 prefix so the role-constraint upgrade stays contiguous.
    c.execute("DROP TABLE dc24h_audit_events");c.execute("DELETE FROM dc24h_schema_migrations WHERE version>=9");
    c.execute("DROP TABLE dc24h_bans");c.execute("DELETE FROM dc24h_schema_migrations WHERE version=8");
    // Preserve old account bytes through the v6 -> v7 constraint migration.
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=7");
    c.execute("ALTER TABLE dc24h_accounts DROP CONSTRAINT dc24h_accounts_role, ADD CONSTRAINT dc24h_accounts_role CHECK (role IN ('registered','moderator','administrator'))");
    for(const auto role:{"registered","moderator","administrator"}) {
        c.execute("INSERT INTO dc24h_accounts(name,role,status,created_at,updated_at) VALUES (?,?,'disabled',1,2)",{std::string("RoleLegacy_")+role,role});
    }
    const auto before_roles=c.query("SELECT id,name,role,status,created_at,updated_at,revision FROM dc24h_accounts ORDER BY id");
    require(detail::run(c,std::span(detail::builtin_migrations).first(7))==1);
    require(c.query("SELECT id,name,role,status,created_at,updated_at,revision FROM dc24h_accounts ORDER BY id").rows==before_roles.rows);
    for(const auto role:{"vip","operator","owner"}) {
        c.execute("INSERT INTO dc24h_accounts(name,role,status,created_at,updated_at) VALUES (?,?,'disabled',0,0)",{std::string("RoleUpgrade_")+role,role});
    }
    try{c.execute("INSERT INTO dc24h_accounts(name,role,status,created_at,updated_at) VALUES ('GuestForbidden','guest','disabled',0,0)");require(false);}catch(const Error& e){require(e.kind()==ErrorKind::constraint);}
    c.execute("DELETE FROM dc24h_accounts WHERE name IN ('RoleUpgrade_vip','RoleUpgrade_operator','RoleUpgrade_owner')");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version=7");
    c.execute("ALTER TABLE dc24h_accounts DROP CONSTRAINT dc24h_accounts_role, ADD CONSTRAINT dc24h_accounts_role CHECK (role IN ('registered','moderator','administrator'))");
    adc_upgrade(c);
    identity_upgrade(c);
    credential_upgrade(c);
    require(detail::run(c,std::span(detail::builtin_migrations).first(7))==1); // Restore the role catalog after historical-prefix tests.
    c.execute("DROP TABLE dc24h_account_adc_credentials");
    c.execute("DROP TABLE dc24h_account_identities");
    c.execute("DROP TABLE dc24h_account_credentials");
    c.execute("DROP TABLE dc24h_accounts");
    c.execute("DELETE FROM dc24h_schema_migrations WHERE version>=3");
    legacy(c); locking(c,options); future(c,options);
    c.execute("DELETE FROM dc24h_schema_migrations");
    rejects(MigrationErrorKind::history,[&] { (void)migrate(c); });
    c.execute("DROP TABLE dc24h_audit_events");
    c.execute("DROP TABLE dc24h_bans");
    c.execute("DROP TABLE dc24h_account_adc_credentials");
    c.execute("DROP TABLE dc24h_account_identities");
    c.execute("DROP TABLE dc24h_account_credentials");
    c.execute("DROP TABLE dc24h_accounts");
    c.execute("DROP TABLE dc24h_schema_migrations");
    require(inspect_migrations(c).current_version == 0 && inspect_migrations(c).pending == 10);
    require(migrate(c) == 10 && migrate(c) == 0);
}
}
int main(int argc, char* argv[]) {
    try {
        if (argc == 1) { unit(); }
        else if (argc == 3) {
            Options options; options.enabled = true; options.socket = argv[1]; options.password_file = argv[2];
            integration(options);
        } else { throw std::runtime_error("Invalid migration test arguments"); }
        std::cout << "Database migration checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
