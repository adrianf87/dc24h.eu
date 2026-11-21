#pragma once

#include "dc24h/db/config.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dc24h::db {

inline constexpr unsigned current_schema = 5;

class Error : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

enum class BanKind { cid, ip };

struct BanInput {
    BanKind kind;
    std::string_view identifier;
    std::string_view reason;
    std::optional<std::uint32_t> duration_seconds;
};

struct Ban {
    std::uint64_t id;
    BanKind kind;
    std::string identifier;
    std::string reason;
    std::string created_at;
    std::optional<std::string> expires_at;
    std::optional<std::string> actor;
};

struct ActiveBan {
    std::optional<std::uint64_t> seconds_remaining;
};

struct AuditEvent {
    std::uint64_t id;
    std::string type;
    std::string created_at;
    std::optional<std::string> actor;
    std::optional<std::string> object_id;
    std::string details;
};

enum class AdminRole { admin, viewer };

struct AdminAccount {
    std::uint64_t id;
    std::string username;
    AdminRole role;
};

struct AdminCredentials {
    AdminAccount account;
    std::string password_hash;
};

[[nodiscard]] bool valid_admin_username(std::string_view name) noexcept;
[[nodiscard]] std::string_view role_name(AdminRole role) noexcept;

using Settings = std::vector<std::pair<std::string, std::string>>;

// One synchronous connection. Construct, use, and destroy it on the same non-I/O thread.
class Store {
  public:
    explicit Store(const Config &config);
    ~Store();

    Store(const Store &) = delete;
    Store &operator=(const Store &) = delete;

    [[nodiscard]] unsigned schema_version();
    void require_current_schema();
    [[nodiscard]] Settings settings();
    void set_setting(std::string_view name, std::string_view value, std::string_view actor = {});

    [[nodiscard]] std::uint64_t add_ban(const BanInput &input, std::string_view actor = {});
    [[nodiscard]] bool remove_ban(std::uint64_t id, std::string_view actor = {});
    [[nodiscard]] std::vector<Ban> bans(std::uint64_t after_id = 0, std::uint64_t limit = 100);
    [[nodiscard]] std::optional<ActiveBan> active_ban(std::string_view cid,
                                                      std::string_view peer_ip);
    [[nodiscard]] std::vector<AuditEvent> audit_events(std::uint64_t after_id = 0,
                                                       std::uint64_t limit = 100);

    void create_admin_user(std::string_view username, AdminRole role, std::string_view hash);
    [[nodiscard]] std::optional<AdminCredentials> admin_credentials(std::string_view username);
    void create_admin_session(const AdminAccount &account, std::string_view digest,
                              std::uint64_t lifetime_seconds);
    [[nodiscard]] std::optional<AdminAccount> admin_session(std::string_view digest);
    void revoke_admin_session(const AdminAccount &account, std::string_view digest);
    [[nodiscard]] std::uint64_t revoke_admin_sessions(std::string_view username);

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Use the migration account. The advisory lock is held until this connection closes.
[[nodiscard]] unsigned migrate(const Config &config);

} // namespace dc24h::db
