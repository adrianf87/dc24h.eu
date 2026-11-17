#pragma once

#include <memory>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <stdexcept>

namespace dc24h::database {

struct Options {
    bool enabled{false};
    // Local Unix sockets only; no TCP fallback or automatic reconnect.
    std::string socket{"/run/mysqld/mysqld.sock"};
    std::string name{"dc24h"};
    std::string user{"dc24h"};
    std::string password_file;
    unsigned int timeout{5};
    // Zero inherits timeout in the C++ API; explicit INI values are 1..60.
    unsigned int connect_timeout{}, read_timeout{}, write_timeout{};
};

void validate(const Options& options);
// Never embeds paths, passwords, SQL or server error text in diagnostics.
[[nodiscard]] std::string read_password(const std::string& filename);

enum class ErrorKind { initialization, credentials, connection, statement, constraint,
    conflict, result_shape, limit, transaction, wrong_thread };
enum class Operation { connect, ping, prepare, bind, execute, fetch, begin, commit, rollback };
// Fixed diagnostics only. Native numeric codes contain no server message/SQL.
// A lost execute/fetch/commit acknowledgement can have an unknown outcome.
class Error final : public std::runtime_error {
public:
    Error(ErrorKind kind, Operation operation, unsigned int native_code = 0, bool outcome_unknown = false);
    [[nodiscard]] ErrorKind kind() const noexcept { return kind_; }
    [[nodiscard]] Operation operation() const noexcept { return operation_; }
    [[nodiscard]] unsigned int native_code() const noexcept { return native_code_; }
    [[nodiscard]] bool outcome_unknown() const noexcept { return outcome_unknown_; }
private:
    ErrorKind kind_;
    Operation operation_;
    unsigned int native_code_;
    bool outcome_unknown_;
};

using Bytes = std::vector<unsigned char>;
using Parameter = std::variant<std::nullptr_t, std::int64_t, std::uint64_t, std::string, Bytes>;
using Parameters = std::vector<Parameter>;
inline constexpr std::size_t max_sql_bytes = 65536, max_parameter_bytes = 65536,
    max_parameters = 256, max_bound_bytes = 1048576;
struct QueryLimits {
    std::size_t rows{256}, columns{32}, cell_bytes{65536}, total_bytes{1048576};
};
void validate(const QueryLimits& limits);
using Cell = std::optional<std::string>; // Exact returned bytes; SQL NULL is distinct from empty.
using Row = std::vector<Cell>;
struct Result { std::vector<std::string> columns; std::vector<Row> rows; };
struct Execution { std::uint64_t affected_rows{}, insert_id{}; };
class Transaction;

// Synchronous, single-thread-owned connection. No use on the Network reactor.
// RAII keeps Connector/C details out of the public interface; implicit reconnect is disabled.
class Connection final {
public:
    explicit Connection(const Options& options);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    void ping();
    // Owner-thread, one explicit attempt with the original settings and a freshly
    // read password file. Closes the old session first; failure leaves it retired.
    // Refuses live Transaction scopes, never replays SQL or restores session state.
    void reconnect();
    [[nodiscard]] Execution execute_prepared(std::string_view sql, const Parameters& parameters = {});
    [[nodiscard]] Result query(std::string_view sql, const Parameters& parameters = {}, QueryLimits limits = {});
    [[nodiscard]] Transaction transaction();
    [[nodiscard]] bool in_transaction() const;
    void execute(std::string_view sql, const std::vector<std::string>& parameters = {});
    // Exactly one row/column, at most 4096 bytes; SQL NULL maps to nullopt.
    [[nodiscard]] std::optional<std::string> scalar(
        std::string_view sql, const std::vector<std::string>& parameters = {});
private:
    friend class Transaction;
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

// One transaction per Connection; no nesting, implicit reconnect or write replay.
// Scope exit rolls back. Connection destruction also rolls back on server close;
// an outstanding Transaction then expires safely. SQL/result errors make it
// rollback-only. Inside a scope, use plain SELECT/INSERT/UPDATE/DELETE/REPLACE/WITH
// statements, without leading comments, DDL or manual transaction control.
class Transaction final {
public:
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&&) = delete;
    void commit();
    void rollback();
private:
    friend class Connection;
    explicit Transaction(const std::shared_ptr<Connection::Impl>& impl);
    std::weak_ptr<Connection::Impl> impl_;
    bool active_{true};
};

struct MigrationInfo { unsigned int version; std::string_view name, checksum; };
struct MigrationStatus {
    unsigned int current_version{}, latest_version{}, pending{};
    bool dirty{false};
};
enum class MigrationErrorKind { catalog, lock_unavailable, history, dirty, schema };
class MigrationError final : public std::runtime_error {
public:
    explicit MigrationError(MigrationErrorKind kind);
    [[nodiscard]] MigrationErrorKind kind() const noexcept { return kind_; }
private:
    MigrationErrorKind kind_;
};
// Compiled catalog only. Views refer to process-lifetime immutable metadata.
[[nodiscard]] std::vector<MigrationInfo> migration_catalog();
// Read-only metadata inspection; no lock acquisition or DDL. Dirty is reported.
[[nodiscard]] MigrationStatus inspect_migrations(Connection& connection);
// Explicit operator action only. Applies a checked contiguous prefix under the
// schema advisory lock; refuses dirty history, never repairs/replays DDL blindly.
[[nodiscard]] unsigned int migrate(Connection& connection);

} // namespace dc24h::database
