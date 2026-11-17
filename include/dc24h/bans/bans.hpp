#pragma once
#include "dc24h/accounts/account.hpp"
#include "dc24h/security/authentication_limits.hpp"
#include <exception>
#include <memory>
#include <unordered_map>
#include <vector>

namespace dc24h::bans {
using Timestamp=accounts::Timestamp;
inline constexpr std::size_t max_active_bans=4096;
enum class Kind { account, cid, ip };
[[nodiscard]] std::string_view kind_key(Kind kind) noexcept;
[[nodiscard]] std::optional<Kind> parse_kind(std::string_view key) noexcept;
[[nodiscard]] std::optional<std::string> canonical_subject(Kind kind,std::string_view text);
[[nodiscard]] std::optional<std::uint64_t> account_subject(std::string_view text) noexcept;
enum class ErrorKind { invalid_input, forbidden, stale, not_found, schema, invalid_record, capacity };
class Error final : public std::exception {
public:
    explicit Error(ErrorKind kind) noexcept : kind_(kind) {}
    [[nodiscard]] ErrorKind kind() const noexcept{return kind_;}
    [[nodiscard]] const char* what() const noexcept override;
private: ErrorKind kind_;
};
struct Record {
    std::uint64_t id{};
    Kind kind{};
    std::string subject,reason;
    Timestamp created_at{};
    std::optional<Timestamp> expires_at;
    std::uint64_t created_by{};
    std::optional<Timestamp> revoked_at;
    std::optional<std::uint64_t> revoked_by;
    std::uint64_t revision{1};
    friend bool operator==(const Record&,const Record&)=default;
};
void validate(const Record& record);
struct Rule {Kind kind;std::string subject;std::optional<Timestamp> expires_at;};
struct Decision {std::optional<Timestamp> expires_at;};
// Immutable startup snapshot; checks allocate nothing and read no SQL/secrets.
// Expiration is evaluated on each admission. Revocation/addition needs restart.
class Directory final {
public:
    explicit Directory(const std::vector<Rule>& rules);
    [[nodiscard]] std::optional<Decision> ip(std::string_view numeric,Timestamp now) const noexcept;
    [[nodiscard]] std::optional<Decision> cid(std::string_view value,Timestamp now) const noexcept;
    [[nodiscard]] std::optional<Decision> account(std::uint64_t id,Timestamp now) const noexcept;
    [[nodiscard]] static Timestamp now() noexcept;
private:
    struct Hash {using is_transparent=void;std::size_t operator()(std::string_view s) const noexcept{return std::hash<std::string_view>{}(s);}};
    std::unordered_map<security::AuthenticationAddress,std::optional<Timestamp>,security::AuthenticationAddressHash> ips_;
    std::unordered_map<std::string,std::optional<Timestamp>,Hash,std::equal_to<>> cids_;
    std::unordered_map<std::uint64_t,std::optional<Timestamp>> accounts_;
};
} // namespace dc24h::bans
