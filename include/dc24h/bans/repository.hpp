#pragma once
#include "dc24h/bans/bans.hpp"
#include "dc24h/accounts/repository.hpp"
namespace dc24h::bans {
class Repository final {
public:
    explicit Repository(database::Connection& connection) noexcept:connection_(connection){}
    void require_schema();
    [[nodiscard]] std::optional<Record> find(std::uint64_t id);
    [[nodiscard]] std::vector<Record> list(std::uint64_t after=0);
    [[nodiscard]] std::shared_ptr<const Directory> load(Timestamp now);
private:
    database::Connection& connection_;
};
// Trusted synchronous stopped-hub maintenance; no replay or credential retention.
class Service final {
public:
    explicit Service(database::Connection& connection) noexcept:connection_(connection){}
    [[nodiscard]] Record create(const accounts::StoredAccount& actor,std::string_view proof,Kind kind,
        std::string_view subject,std::string_view reason,std::optional<Timestamp> expires,Timestamp now);
    [[nodiscard]] Record revoke(const accounts::StoredAccount& actor,std::string_view proof,const Record& expected,Timestamp now);
private:
    [[nodiscard]] Record perform(const accounts::StoredAccount& actor,std::string_view proof,Record candidate,bool revoke,Timestamp now);
    database::Connection& connection_;
};
}
