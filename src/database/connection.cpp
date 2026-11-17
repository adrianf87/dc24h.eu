#include "dc24h/database/database.hpp"

#include <mysql.h>
#include <errmsg.h>
#include <openssl/crypto.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>

namespace dc24h::database {
namespace {
const char* diagnostic(ErrorKind kind) noexcept {
    switch (kind) {
    case ErrorKind::initialization: return "Database initialization failed";
    case ErrorKind::credentials: return "Cannot read private database password file";
    case ErrorKind::connection: return "Database connection is unusable";
    case ErrorKind::statement: return "Database statement failed";
    case ErrorKind::constraint: return "Database constraint violation";
    case ErrorKind::conflict: return "Database transaction conflict";
    case ErrorKind::result_shape: return "Database result shape invalid";
    case ErrorKind::limit: return "Database operation limit exceeded";
    case ErrorKind::transaction: return "Invalid database transaction state";
    case ErrorKind::wrong_thread: return "Database connection used from a different thread";
    }
    return "Database statement failed";
}
void library() {
    struct Runtime {
        Runtime() { if (mysql_library_init(0, nullptr, nullptr) != 0) { throw Error(ErrorKind::initialization, Operation::connect); } }
        ~Runtime() { mysql_library_end(); }
    };
    // C++ serializes initialization before any of this module's mysql_init calls.
    // All connection-owning threads must finish before process/static teardown.
    static const Runtime runtime;
    (void)runtime;
}
bool lost(unsigned int code, const char* state) noexcept {
    return code == CR_SERVER_GONE_ERROR || code == CR_SERVER_LOST ||
        code == CR_COMMANDS_OUT_OF_SYNC || code == CR_MALFORMED_PACKET ||
        code == CR_ERR_NET_READ || code == CR_ERR_NET_WRITE || code == CR_ERR_NET_UNCOMPRESS ||
        code == CR_SERVER_LOST_EXTENDED || (state && state[0] == '0' && state[1] == '8');
}
Error native_error(unsigned int code, const char* state, Operation operation) {
    auto kind = ErrorKind::statement;
    if (lost(code, state)) { kind = ErrorKind::connection; }
    else if (code == 1205 || code == 1213) { kind = ErrorKind::conflict; }
    else if (state && state[0] == '2' && state[1] == '3') { kind = ErrorKind::constraint; }
    const bool uncertain = kind == ErrorKind::connection &&
        (operation == Operation::execute || operation == Operation::fetch || operation == Operation::commit);
    return Error(kind, operation, code, uncertain);
}
void check_parameters(std::string_view sql, const Parameters& parameters) {
    if (sql.empty() || sql.size() > max_sql_bytes || sql.find('\0') != std::string_view::npos || parameters.size() > max_parameters) {
        throw Error(ErrorKind::limit, Operation::prepare);
    }
    std::size_t total = 0;
    for (const auto& parameter : parameters) {
        const auto bytes = std::visit([](const auto& value) -> std::size_t {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, Bytes>) { return value.size(); }
            else if constexpr (std::is_same_v<T, std::nullptr_t>) { return 0; }
            else { return sizeof(value); }
        }, parameter);
        if (bytes > max_parameter_bytes || bytes > max_bound_bytes - total) { throw Error(ErrorKind::limit, Operation::bind); }
        total += bytes;
    }
}
Parameters text_parameters(const std::vector<std::string>& values) {
    if (values.size() > max_parameters) { throw Error(ErrorKind::limit, Operation::bind); }
    std::size_t bytes = 0;
    for (const auto& value : values) {
        if (value.size() > max_parameter_bytes || value.size() > max_bound_bytes - bytes) { throw Error(ErrorKind::limit, Operation::bind); }
        bytes += value.size();
    }
    Parameters result;
    result.reserve(values.size());
    for (const auto& value : values) { result.emplace_back(value); }
    return result;
}
bool transactional_sql(std::string_view sql) noexcept {
    while (!sql.empty() && (sql.front() == ' ' || sql.front() == '\t' || sql.front() == '\r' || sql.front() == '\n')) { sql.remove_prefix(1); }
    const auto upper = [](char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c; };
    std::size_t end = 0;
    while (end < sql.size() && upper(sql[end]) >= 'A' && upper(sql[end]) <= 'Z') { ++end; }
    const auto word = sql.substr(0, end);
    for (const auto allowed : {"SELECT", "INSERT", "UPDATE", "DELETE", "REPLACE", "WITH"}) {
        const std::string_view name(allowed);
        if (word.size() == name.size() && std::equal(word.begin(), word.end(), name.begin(), [&](char a, char b) { return upper(a) == b; })) { return true; }
    }
    return false;
}
} // namespace

Error::Error(ErrorKind kind, Operation operation, unsigned int native_code, bool outcome_unknown)
    : std::runtime_error(diagnostic(kind)), kind_(kind), operation_(operation), native_code_(native_code), outcome_unknown_(outcome_unknown) {}

void validate(const QueryLimits& limits) {
    if (limits.rows > 1024 || limits.columns == 0 || limits.columns > 64 ||
        limits.cell_bytes > 65536 || limits.total_bytes > 4 * 1024 * 1024) {
        throw Error(ErrorKind::limit, Operation::fetch);
    }
}

struct Connection::Impl {
    const std::thread::id owner{std::this_thread::get_id()};
    const Options options; // Paths/settings only; never cache the application password.
    MYSQL* handle{};
    bool transaction_active{false}, transaction_failed{false};
    std::atomic<bool> retire_requested{false};
    std::atomic<bool> scope_active{false}; // Survives transport retirement until scope cleanup.
    explicit Impl(const Options& settings) : options(settings) {
        library();
        handle = mysql_init(nullptr);
        if (!handle) { throw Error(ErrorKind::initialization, Operation::connect); }
    }
    ~Impl() { if (handle) { mysql_close(handle); } }
    void retire() noexcept {
        if (handle) { mysql_close(handle); handle = nullptr; }
        transaction_active = transaction_failed = false;
    }
    void check(Operation operation, bool allow_failed = false) {
        // Check immutable ownership before touching owner-thread mutable state.
        if (owner != std::this_thread::get_id()) { throw Error(ErrorKind::wrong_thread, operation); }
        if (retire_requested.load()) { retire(); }
        if (!handle) { throw Error(ErrorKind::connection, operation); }
        if (transaction_failed && !allow_failed) { throw Error(ErrorKind::transaction, operation); }
    }
    void after(Operation operation) {
        if (retire_requested.load()) { retire(); throw Error(ErrorKind::connection, operation, 0, operation == Operation::execute || operation == Operation::commit); }
        unsigned int status{};
        if (mariadb_get_infov(handle, MARIADB_CONNECTION_SERVER_STATUS, &status) != 0) {
            retire(); throw Error(ErrorKind::connection, operation);
        }
        if (!(status & SERVER_STATUS_AUTOCOMMIT) || bool(status & SERVER_STATUS_IN_TRANS) != transaction_active) {
            retire(); throw Error(ErrorKind::transaction, operation);
        }
    }
    void failure(ErrorKind kind) noexcept {
        if (!handle) { return; }
        if (kind == ErrorKind::connection || retire_requested.load()) { retire();return; }
        // Even a result-shape/allocation error may follow successful SQL. Do
        // not retain an unowned transaction or disabled autocommit session.
        unsigned int status{};
        if (mariadb_get_infov(handle, MARIADB_CONNECTION_SERVER_STATUS, &status) != 0 ||
            !(status & SERVER_STATUS_AUTOCOMMIT) || (!transaction_active && (status & SERVER_STATUS_IN_TRANS))) {
            retire();return;
        }
        if (transaction_active) { transaction_failed = true; }
    }
    template<class F> auto run(Operation operation, F&& function) {
        check(operation);
        try { auto result = function(); after(operation); return result; }
        catch (const Error& error) { failure(error.kind()); throw; }
        catch (...) { failure(ErrorKind::statement); throw; }
    }
    void control(std::string_view sql, Operation operation) {
        if (mysql_real_query(handle, sql.data(), static_cast<unsigned long>(sql.size())) != 0) {
            throw native_error(mysql_errno(handle), mysql_sqlstate(handle), operation);
        }
    }
    struct Statement {
        Impl& connection;
        MYSQL_STMT* handle{};
        std::vector<MYSQL_BIND> bindings;
        std::vector<unsigned long> lengths;
        std::vector<long long> signed_values;
        std::vector<unsigned long long> unsigned_values;
        char empty{};
        Statement(Impl& state, std::string_view sql, const Parameters& parameters) : connection(state) {
            check_parameters(sql, parameters);
            if (state.transaction_active && !transactional_sql(sql)) { throw Error(ErrorKind::transaction, Operation::prepare); }
            handle = mysql_stmt_init(state.handle);
            if (!handle) { throw Error(ErrorKind::initialization, Operation::prepare); }
            try {
                if (mysql_stmt_prepare(handle, sql.data(), static_cast<unsigned long>(sql.size())) != 0) { fail(Operation::prepare); }
                if (mysql_stmt_param_count(handle) != parameters.size()) { throw Error(ErrorKind::statement, Operation::bind); }
                bindings.resize(parameters.size()); lengths.resize(parameters.size());
                signed_values.resize(parameters.size()); unsigned_values.resize(parameters.size());
                for (std::size_t i = 0; i < parameters.size(); ++i) {
                    auto& bind = bindings[i];
                    std::visit([&](const auto& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, std::nullptr_t>) { bind.buffer_type = MYSQL_TYPE_NULL; }
                        else if constexpr (std::is_same_v<T, std::int64_t> || std::is_same_v<T, std::uint64_t>) {
                            bind.buffer_type = MYSQL_TYPE_LONGLONG;
                            bind.is_unsigned = std::is_same_v<T, std::uint64_t>;
                            if constexpr (std::is_same_v<T, std::int64_t>) {
                                signed_values[i] = static_cast<long long>(value);
                                bind.buffer = &signed_values[i]; bind.buffer_length = sizeof(signed_values[i]);
                            } else {
                                unsigned_values[i] = static_cast<unsigned long long>(value);
                                bind.buffer = &unsigned_values[i]; bind.buffer_length = sizeof(unsigned_values[i]);
                            }
                        } else {
                            bind.buffer_type = std::is_same_v<T, Bytes> ? MYSQL_TYPE_BLOB : MYSQL_TYPE_STRING;
                            lengths[i] = static_cast<unsigned long>(value.size());
                            bind.buffer = value.empty() ? static_cast<void*>(&empty) : const_cast<void*>(static_cast<const void*>(value.data()));
                            bind.buffer_length = lengths[i]; bind.length = &lengths[i];
                        }
                    }, parameters[i]);
                }
                if (!bindings.empty() && mysql_stmt_bind_param(handle, bindings.data()) != 0) { fail(Operation::bind); }
                if (mysql_stmt_execute(handle) != 0) { fail(Operation::execute); }
            } catch (...) { close(); throw; }
        }
        [[noreturn]] void fail(Operation operation) const { throw native_error(mysql_stmt_errno(handle), mysql_stmt_sqlstate(handle), operation); }
        void close() noexcept {
            if (handle) {
                if (mysql_stmt_close(handle) != 0) { connection.retire_requested.store(true); }
                handle = nullptr;
            }
        }
        ~Statement() { close(); }
        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;
    };
};

Connection::Connection(const Options& options) {
    validate(options);
    if (!options.enabled) { throw std::invalid_argument("Database is disabled"); }
    impl_ = std::make_shared<Impl>(options);
    auto* handle = impl_->handle;
    const auto option = [&](mysql_option key, const void* value) {
        if (mysql_optionsv(handle, key, value) != 0) { throw Error(ErrorKind::initialization, Operation::connect); }
    };
    const auto connect_timeout = options.connect_timeout ? options.connect_timeout : options.timeout;
    const auto read_timeout = options.read_timeout ? options.read_timeout : options.timeout;
    const auto write_timeout = options.write_timeout ? options.write_timeout : options.timeout;
    option(MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout);
    option(MYSQL_OPT_READ_TIMEOUT, &read_timeout);
    option(MYSQL_OPT_WRITE_TIMEOUT, &write_timeout);
    const my_bool off = 0;
    const unsigned int local_infile = 0, protocol = MYSQL_PROTOCOL_SOCKET;
    option(MYSQL_OPT_PROTOCOL, &protocol); option(MYSQL_OPT_RECONNECT, &off);
    option(MYSQL_OPT_SSL_ENFORCE, &off); option(MYSQL_OPT_LOCAL_INFILE, &local_infile);
    option(MYSQL_SET_CHARSET_NAME, "utf8mb4");
    auto password = [&] {
        try { return read_password(options.password_file); }
        catch (const std::bad_alloc&) { throw; }
        catch (const std::exception&) { throw Error(ErrorKind::credentials, Operation::connect); }
    }();
    struct Erase { std::string& value; ~Erase() { OPENSSL_cleanse(value.data(), value.size()); } } erase{password};
    if (!mysql_real_connect(handle, "localhost", options.user.c_str(), password.c_str(), options.name.c_str(), 0, options.socket.c_str(), 0)) {
        throw Error(ErrorKind::connection, Operation::connect, mysql_errno(handle));
    }
    if (mysql_set_character_set(handle, "utf8mb4") != 0 || mysql_autocommit(handle, 1) != 0) {
        throw Error(ErrorKind::connection, Operation::connect, mysql_errno(handle));
    }
    impl_->after(Operation::connect);
}
Connection::~Connection() = default;
void Connection::reconnect() {
    if (impl_->owner != std::this_thread::get_id()) { throw Error(ErrorKind::wrong_thread, Operation::connect); }
    if (impl_->scope_active.load()) { throw Error(ErrorKind::transaction, Operation::connect); }
    // Expire all old weak Transaction links only after a fully initialized new
    // session is available. On any failure the original session stays retired.
    impl_->retire();
    Connection replacement(impl_->options);
    impl_.swap(replacement.impl_);
}
void Connection::ping() {
    (void)impl_->run(Operation::ping, [&] {
        if (mysql_ping(impl_->handle) != 0) { throw native_error(mysql_errno(impl_->handle), mysql_sqlstate(impl_->handle), Operation::ping); }
        return 0;
    });
}
Execution Connection::execute_prepared(std::string_view sql, const Parameters& parameters) {
    return impl_->run(Operation::execute, [&] {
        const Impl::Statement statement(*impl_, sql, parameters);
        if (mysql_stmt_field_count(statement.handle) != 0 || mysql_stmt_more_results(statement.handle)) { throw Error(ErrorKind::result_shape, Operation::execute); }
        const auto affected = mysql_stmt_affected_rows(statement.handle);
        if (affected == std::numeric_limits<my_ulonglong>::max()) { statement.fail(Operation::execute); }
        return Execution{static_cast<std::uint64_t>(affected), static_cast<std::uint64_t>(mysql_stmt_insert_id(statement.handle))};
    });
}
Result Connection::query(std::string_view sql, const Parameters& parameters, QueryLimits limits) {
    return impl_->run(Operation::fetch, [&] {
        validate(limits);
        const Impl::Statement statement(*impl_, sql, parameters);
        const auto count = mysql_stmt_field_count(statement.handle);
        if (!count) { throw Error(ErrorKind::result_shape, Operation::fetch); }
        if (count > limits.columns) { throw Error(ErrorKind::limit, Operation::fetch); }
        const std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> metadata(mysql_stmt_result_metadata(statement.handle), mysql_free_result);
        if (!metadata) { statement.fail(Operation::fetch); }
        const auto* fields = mysql_fetch_fields(metadata.get());
        if (!fields) { throw Error(ErrorKind::result_shape, Operation::fetch); }
        Result result;
        for (unsigned int i = 0; i < count; ++i) {
            if (fields[i].name_length > 1024 || (!fields[i].name && fields[i].name_length)) { throw Error(ErrorKind::limit, Operation::fetch); }
            result.columns.emplace_back(fields[i].name_length ? std::string(fields[i].name, fields[i].name_length) : std::string{});
        }
        struct Column { char empty{}; unsigned long length{}; my_bool is_null{}, error{}; };
        std::vector<Column> columns(count);
        std::vector<MYSQL_BIND> bindings(count);
        for (unsigned int i = 0; i < count; ++i) {
            auto& bind = bindings[i];auto& column = columns[i];
            bind.buffer_type = MYSQL_TYPE_STRING;bind.buffer = &column.empty;bind.buffer_length = 0;
            bind.length = &column.length;bind.is_null = &column.is_null;bind.error = &column.error;
        }
        if (mysql_stmt_bind_result(statement.handle, bindings.data()) != 0) { statement.fail(Operation::bind); }
        std::size_t total = 0;
        for (;;) {
            const auto status = mysql_stmt_fetch(statement.handle);
            if (status == MYSQL_NO_DATA) { break; }
            if (status != 0 && status != MYSQL_DATA_TRUNCATED) { statement.fail(Operation::fetch); }
            if (result.rows.size() == limits.rows) { throw Error(ErrorKind::limit, Operation::fetch); }
            Row row;
            row.reserve(count);
            for (unsigned int i = 0; i < count; ++i) {
                if (columns[i].is_null) { row.emplace_back(std::nullopt);continue; }
                const auto length = columns[i].length;
                if (length > limits.cell_bytes || length > limits.total_bytes - total) { throw Error(ErrorKind::limit, Operation::fetch); }
                std::string value(length, '\0');
                if (length) {
                    unsigned long actual{};my_bool is_null{}, error{};
                    MYSQL_BIND bind{};
                    bind.buffer_type=MYSQL_TYPE_STRING;bind.buffer=value.data();bind.buffer_length=length;
                    bind.length=&actual;bind.is_null=&is_null;bind.error=&error;
                    if (mysql_stmt_fetch_column(statement.handle, &bind, i, 0) != 0) { statement.fail(Operation::fetch); }
                    if (is_null || error || actual != length) { throw Error(ErrorKind::result_shape, Operation::fetch); }
                }
                total += length;row.emplace_back(std::move(value));
            }
            result.rows.push_back(std::move(row));
        }
        if (mysql_stmt_more_results(statement.handle)) { throw Error(ErrorKind::result_shape, Operation::fetch); }
        return result;
    });
}
void Connection::execute(std::string_view sql, const std::vector<std::string>& parameters) {
    (void)impl_->run(Operation::execute, [&] {
        (void)execute_prepared(sql, text_parameters(parameters));
        return 0;
    });
}
std::optional<std::string> Connection::scalar(std::string_view sql, const std::vector<std::string>& parameters) {
    return impl_->run(Operation::fetch, [&] {
        auto result = query(sql, text_parameters(parameters), QueryLimits{1, 1, 4096, 4096});
        if (result.rows.size() != 1) { throw Error(ErrorKind::result_shape, Operation::fetch); }
        return std::move(result.rows.front().front());
    });
}
bool Connection::in_transaction() const { impl_->check(Operation::begin, true); return impl_->transaction_active; }
Transaction Connection::transaction() { return Transaction(impl_); }

Transaction::Transaction(const std::shared_ptr<Connection::Impl>& impl) : impl_(impl) {
    impl->check(Operation::begin);
    if (impl->transaction_active) { throw Error(ErrorKind::transaction, Operation::begin); }
    try {
        impl->control("START TRANSACTION", Operation::begin);
        impl->transaction_active = true;
        impl->after(Operation::begin);
        impl->scope_active.store(true);
    } catch (...) { impl->retire(); throw; }
}
Transaction::Transaction(Transaction&& other) noexcept : impl_(std::move(other.impl_)), active_(std::exchange(other.active_, false)) {}
void Transaction::commit() {
    const auto impl = impl_.lock();
    if (!impl) { throw Error(ErrorKind::transaction, Operation::commit); }
    impl->check(Operation::commit, true);
    if (!active_ || !impl->transaction_active || impl->transaction_failed) { throw Error(ErrorKind::transaction, Operation::commit); }
    try {
        impl->control("COMMIT", Operation::commit);
        impl->transaction_active = false;active_ = false;impl->scope_active.store(false);
        impl->after(Operation::commit);
    } catch (...) { impl->retire();active_ = false;impl->scope_active.store(false);throw; }
}
void Transaction::rollback() {
    const auto impl = impl_.lock();
    if (!impl) { throw Error(ErrorKind::transaction, Operation::rollback); }
    impl->check(Operation::rollback, true);
    if (!active_ || !impl->transaction_active) { throw Error(ErrorKind::transaction, Operation::rollback); }
    try {
        impl->control("ROLLBACK", Operation::rollback);
        impl->transaction_active = impl->transaction_failed = false;active_ = false;impl->scope_active.store(false);
        impl->after(Operation::rollback);
    } catch (...) { impl->retire();active_ = false;impl->scope_active.store(false);throw; }
}
Transaction::~Transaction() {
    if (!active_) { return; }
    const auto impl = impl_.lock();
    if (!impl) { return; } // Connection close already rolled back the server transaction.
    if (impl->owner != std::this_thread::get_id()) {
        impl->retire_requested.store(true);impl->scope_active.store(false);return;
    }
    try { rollback(); } catch (...) { impl->retire(); }
    impl->scope_active.store(false);
}
} // namespace dc24h::database
