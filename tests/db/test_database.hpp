#pragma once

#include "dc24h/db/config.hpp"

#include <mysql.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace dc24h::db::test {

inline std::filesystem::path fixture_path(std::string_view name) {
    const char *directory = std::getenv("DC24H_TEST_DB_DIR");
    if (!directory || !std::string_view(directory).starts_with("/tmp/dc24h-e5.")) {
        throw std::runtime_error("Run this test suite through the isolated MariaDB fixture");
    }
    return std::filesystem::path(directory) / name;
}

inline std::string read_fixture(std::string_view name) {
    std::ifstream file(fixture_path(name));
    std::string value;
    if (!std::getline(file, value) || value.empty()) {
        throw std::runtime_error("Could not read fixture file");
    }
    return value;
}

inline Config credentials(std::string_view role, std::string_view database = "dc24h_e5_test") {
    return {std::string(database), "e5_" + std::string(role),
            read_fixture(std::string(role) + ".pw"), fixture_path("mdb.sock")};
}

class TestSql {
  public:
    explicit TestSql(const char *database = "dc24h_e5_test") {
        if (!mysql_real_connect(connection_.get(), "localhost", "root", "", database, 0,
                                fixture_path("mdb.sock").c_str(), 0)) {
            throw std::runtime_error("Could not connect to the fixture as its administrator");
        }
    }

    void execute(const char *sql) {
        if (mysql_query(connection_.get(), sql) != 0) {
            throw std::runtime_error(mysql_error(connection_.get()));
        }
        if (auto *result = mysql_store_result(connection_.get())) {
            mysql_free_result(result);
        }
    }

  private:
    std::unique_ptr<MYSQL, decltype(&mysql_close)> connection_{mysql_init(nullptr), &mysql_close};
};

} // namespace dc24h::db::test
