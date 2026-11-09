#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dc24h::database {

struct Options {
    bool enabled{false};
    // First database increment supports local Unix sockets only.
    std::string socket{"/run/mysqld/mysqld.sock"};
    std::string name{"dc24h"};
    std::string user{"dc24h"};
    std::string password_file;
    unsigned int timeout{5};
};

void validate(const Options& options);
// Never embeds paths, passwords, SQL or server error text in diagnostics.
[[nodiscard]] std::string read_password(const std::string& filename);

// Synchronous, single-thread-owned connection. No use on the Network reactor.
// RAII keeps Connector/C details out of the public interface; reconnect is disabled.
class Connection final {
public:
    explicit Connection(const Options& options);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    void ping();
    void execute(std::string_view sql, const std::vector<std::string>& parameters = {});
    // Exactly one row/column, at most 4096 bytes; SQL NULL maps to nullopt.
    [[nodiscard]] std::optional<std::string> scalar(
        std::string_view sql, const std::vector<std::string>& parameters = {});
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Explicit operator action only. Returns number of newly applied migrations.
// Baseline creates migration metadata only, no user/authentication schema yet.
[[nodiscard]] unsigned int migrate(Connection& connection);

} // namespace dc24h::database
