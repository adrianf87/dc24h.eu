#pragma once

#include <string>

namespace dc24h::db {

struct Config {
    std::string database;
    std::string user;
    std::string password;
    std::string socket = "/run/mysqld/mysqld.sock";
};

} // namespace dc24h::db
