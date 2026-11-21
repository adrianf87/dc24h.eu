#include "database_cli.hpp"

#include "dc24h/admin/crypto.hpp"
#include "dc24h/db/store.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <ostream>

namespace dc24h {
namespace {

std::string environment(const char *name) {
    if (const auto *value = std::getenv(name)) {
        return value;
    }
    return {};
}

struct PasswordFile {
    explicit PasswordFile(const char *path)
        : fd(open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY)) {}
    ~PasswordFile() {
        if (fd >= 0) {
            close(fd);
        }
    }

    PasswordFile(const PasswordFile &) = delete;
    PasswordFile &operator=(const PasswordFile &) = delete;
    int fd;
};

std::string read_password(const std::string &path) {
    const PasswordFile file(path.c_str());
    struct stat status {};
    if (file.fd < 0 || fstat(file.fd, &status) != 0 || !S_ISREG(status.st_mode) ||
        (status.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        throw std::invalid_argument("Password file must be a private regular file");
    }
    constexpr std::size_t max_password_bytes = 4096;
    std::array<char, max_password_bytes + 3> bytes;
    std::size_t size = 0;
    while (size < bytes.size()) {
        const auto count = read(file.fd, bytes.data() + size, bytes.size() - size);
        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::invalid_argument("Could not read password file");
        }
        size += static_cast<std::size_t>(count);
    }
    std::string_view value(bytes.data(), size);
    if (value.ends_with('\n')) {
        value.remove_suffix(1);
        if (value.ends_with('\r')) {
            value.remove_suffix(1);
        }
    }
    if (value.empty() || value.size() > max_password_bytes ||
        value.find_first_of(std::string_view("\r\n\0", 3)) != value.npos) {
        throw std::invalid_argument("Password file must contain one password");
    }
    return std::string(value);
}

template <class Integer> Integer positive_number(std::string_view value) {
    Integer number = 0;
    if (!value.empty()) {
        const auto end = value.data() + value.size();
        const auto parsed = std::from_chars(value.data(), end, number);
        if (parsed.ec == std::errc{} && parsed.ptr == end && number > 0) {
            return number;
        }
    }
    throw std::invalid_argument("Expected a positive integer in range");
}

std::string tsv(std::string_view value) {
    std::string escaped;
    for (const char byte : value) {
        switch (byte) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\t':
            escaped += "\\t";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\n':
            escaped += "\\n";
            break;
        default:
            const auto character = static_cast<unsigned char>(byte);
            escaped += character < 32 || character == 127 ? '?' : byte;
        }
    }
    return escaped;
}

using Arguments = std::span<const std::string_view>;

std::string_view take_actor(Arguments &arguments, std::size_t required) {
    if (arguments.size() == required) {
        return {};
    }
    if (arguments.size() == required + 2 && arguments[required] == "--actor" &&
        !arguments.back().empty()) {
        const auto actor = arguments.back();
        arguments = arguments.first(required);
        return actor;
    }
    throw std::invalid_argument("Invalid database command arguments");
}

int change_ban(db::Store &store, Arguments arguments, std::ostream &output, std::ostream &errors) {
    if (arguments[1] == "add") {
        const auto actor = take_actor(arguments, 6);
        db::BanKind kind;
        if (arguments[2] == "cid") {
            kind = db::BanKind::cid;
        } else if (arguments[2] == "ip") {
            kind = db::BanKind::ip;
        } else {
            throw std::invalid_argument("Expected ban kind cid or ip");
        }
        std::optional<std::uint32_t> duration;
        if (arguments[4] != "permanent") {
            duration = positive_number<std::uint32_t>(arguments[4]);
        }
        const auto id = store.add_ban({kind, arguments[3], arguments[5], duration}, actor);
        output << "Ban " << id << " added\n";
        return 0;
    }
    if (arguments[1] == "remove") {
        const auto actor = take_actor(arguments, 3);
        const auto id = positive_number<std::uint64_t>(arguments[2]);
        if (store.remove_ban(id, actor)) {
            output << "Ban " << id << " removed\n";
            return 0;
        }
        errors << "dc24h-hub: no ban with that ID\n";
        return 1;
    }
    throw std::invalid_argument("Unknown bans command");
}

int database_command(db::Store &store, Arguments arguments, std::ostream &output,
                     std::ostream &errors) {
    if (arguments.size() == 1 && arguments[0] == "status") {
        output << "MariaDB schema " << store.schema_version() << " / " << db::current_schema
               << '\n';
        return 0;
    }
    if (arguments.size() < 2) {
        throw std::invalid_argument("Expected a database command");
    }
    store.require_current_schema();
    const auto group = arguments[0];
    const auto command = arguments[1];
    if (group == "admin" && command == "create" && arguments.size() == 6 &&
        arguments[4] == "--password-file") {
        const auto name = arguments[2];
        const auto role = arguments[3];
        if (!db::valid_admin_username(name) || (role != "admin" && role != "viewer")) {
            throw std::invalid_argument("Invalid administrator username or role");
        }
        const auto password = read_password(std::string(arguments[5]));
        store.create_admin_user(name,
                                role == "admin" ? db::AdminRole::admin : db::AdminRole::viewer,
                                admin::hash_password(password));
        output << "Administrator account created\n";
        return 0;
    }
    if (group == "admin" && command == "revoke" && arguments.size() == 3) {
        const auto count = store.revoke_admin_sessions(arguments[2]);
        output << count << " administrator sessions revoked\n";
        return 0;
    }
    if (command == "list" && arguments.size() <= 3) {
        const auto after = arguments.size() == 3 ? positive_number<std::uint64_t>(arguments[2]) : 0;
        if (group == "settings" && arguments.size() == 2) {
            output << "name\tvalue\n";
            for (const auto &[name, value] : store.settings()) {
                output << tsv(name) << '\t' << tsv(value) << '\n';
            }
            return 0;
        }
        if (group == "bans") {
            output << "id\tkind\tidentifier\treason\tcreated_utc\texpires_utc\tactor\n";
            for (const auto &ban : store.bans(after)) {
                output << ban.id << '\t' << (ban.kind == db::BanKind::cid ? "cid" : "ip") << '\t'
                       << tsv(ban.identifier) << '\t' << tsv(ban.reason) << '\t' << ban.created_at
                       << '\t' << ban.expires_at.value_or("") << '\t' << tsv(ban.actor.value_or(""))
                       << '\n';
            }
            return 0;
        }
        if (group == "audit") {
            output << "id\ttype\tcreated_utc\tactor\tobject_id\tdetails\n";
            for (const auto &event : store.audit_events(after)) {
                output << event.id << '\t' << tsv(event.type) << '\t' << event.created_at << '\t'
                       << tsv(event.actor.value_or("")) << '\t' << tsv(event.object_id.value_or(""))
                       << '\t' << tsv(event.details) << '\n';
            }
            return 0;
        }
    }
    if (group == "settings" && command == "set") {
        const auto actor = take_actor(arguments, 4);
        store.set_setting(arguments[2], arguments[3], actor);
        output << "Hub setting saved\n";
        return 0;
    }
    if (group == "bans") {
        return change_ban(store, arguments, output, errors);
    }
    throw std::invalid_argument("Unknown database command");
}

} // namespace

db::Config read_database_config(DatabaseAccount account) {
    const bool api = account == DatabaseAccount::api;
    db::Config config;
    config.database = environment("DC24H_DB_NAME");
    config.user = environment(api ? "DC24H_API_DB_USER" : "DC24H_DB_USER");
    const auto path = environment(api ? "DC24H_API_DB_PASSWORD_FILE" : "DC24H_DB_PASSWORD_FILE");
    if (config.database.empty() || config.user.empty() || path.empty()) {
        throw std::invalid_argument(
            api ? "Set DC24H_DB_NAME, DC24H_API_DB_USER and DC24H_API_DB_PASSWORD_FILE"
                : "Set DC24H_DB_NAME, DC24H_DB_USER and DC24H_DB_PASSWORD_FILE");
    }
    if (auto socket = environment("DC24H_DB_SOCKET"); !socket.empty()) {
        config.socket = std::move(socket);
    }
    config.password = read_password(path);
    return config;
}

void write_database_help(std::ostream &output) {
    output << "       dc24h-hub --db COMMAND\n"
           << "  migrate, status\n"
           << "  settings list | set NAME VALUE [--actor ACTOR]\n"
           << "  bans list [AFTER_ID]\n"
           << "  bans add cid|ip IDENTIFIER SECONDS|permanent REASON [--actor ACTOR]\n"
           << "  bans remove ID [--actor ACTOR]\n"
           << "  audit list [AFTER_ID]\n"
           << "  admin create NAME admin|viewer --password-file FILE\n"
           << "  admin revoke NAME\n"
           << "  Requires DC24H_DB_NAME, DC24H_DB_USER and DC24H_DB_PASSWORD_FILE.\n";
}

int run_database(Arguments arguments, std::ostream &output, std::ostream &errors) {
    if (arguments.size() == 1 && arguments.front() == "--help") {
        output << "Usage:\n";
        write_database_help(output);
        return 0;
    }
    try {
        if (arguments.empty()) {
            throw std::invalid_argument("Expected a database command");
        }
        const auto config = read_database_config();
        if (arguments.size() == 1 && arguments.front() == "migrate") {
            const auto version = db::migrate(config);
            output << "MariaDB schema " << version << " ready\n";
            return 0;
        }
        db::Store store(config);
        return database_command(store, arguments, output, errors);
    } catch (const std::invalid_argument &error) {
        errors << "dc24h-hub: " << error.what() << ". Try 'dc24h-hub --db --help'.\n";
        return 2;
    } catch (const std::exception &error) {
        errors << "dc24h-hub: " << error.what() << '\n';
        return 1;
    }
}

} // namespace dc24h
