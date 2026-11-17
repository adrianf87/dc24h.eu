#pragma once

#include "dc24h/chat/statistics.hpp"
#include "dc24h/admin/kick.hpp"
#include "dc24h/transfers/statistics.hpp"
#include "dc24h/transfers/security.hpp"
#include "dc24h/search/statistics.hpp"
#include "dc24h/search/security.hpp"
#include "dc24h/api/configuration.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace dc24h::logging { class Logger; }

namespace dc24h::api {
struct Request;
struct Options {
    bool enabled{false};
    std::string socket_path;
    std::string token_file;
    unsigned int max_connections{16};
    unsigned int request_timeout{2};
};
void validate(const Options& options);

struct Snapshot {
    bool core_running{};
    std::uint64_t uptime_seconds{};
    std::uint64_t active_connections{};
    std::uint64_t accepted_connections{};
    std::uint64_t adc_frames{};
    std::uint64_t adcs_handshakes{};
    std::uint64_t security_rejected{};
    std::uint64_t security_closed{};
    bool database_enabled{};
    std::uint64_t rejected_connections{}, closed_connections{}, received_bytes{}, sent_bytes{};
    std::uint64_t peak_connections{}, online_users{}, peak_online_users{}, successful_logins{};
    chat::Statistics chat{};
    search::Statistics search{};
    search::SecuritySettings search_settings{};
    transfers::Statistics transfers{};
    transfers::SecuritySettings transfer_settings{};
    std::optional<ConfigurationSnapshot> configuration{};
    admin::KickStatistics kick{};
};
using StatusProvider = std::function<Snapshot()>;
using FailureHandler = std::function<void()>;
using AccountHandler = std::function<std::string(const Request&)>;

// Owns one local HTTP worker. Call start() once; destructor stops, joins and
// removes only this instance's socket inode. Dependencies/callback targets must
// outlive the Server, and callback work must be bounded and thread safe.
class Server final {
public:
    Server(logging::Logger& logger, Options options, StatusProvider status, FailureHandler failure,
        AccountHandler accounts = {});
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    void start();
    [[nodiscard]] bool failed() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace dc24h::api
