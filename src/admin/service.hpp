#pragma once

#include "dc24h/admin/server.hpp"

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <chrono>
#include <map>
#include <mutex>
#include <string_view>

namespace dc24h::admin {

namespace http = boost::beast::http;
using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;

[[nodiscard]] Response error_response(http::status status, std::string_view code,
                                      std::string_view message);

class Service {
  public:
    Service(net::Server &hub, db::Config database, std::string origin);

    [[nodiscard]] Response respond(const Request &request);

  private:
    [[nodiscard]] Response dispatch(const Request &request);
    [[nodiscard]] Response login(const Request &request);
    [[nodiscard]] bool allow_login(std::string_view username);

    net::Server &hub_;
    db::Config database_;
    std::string origin_;
    std::string dummy_hash_;
    std::mutex login_mutex_;
    std::chrono::steady_clock::time_point login_window_;
    std::map<std::string, unsigned, std::less<>> attempts_;
    unsigned total_attempts_ = 0;
};

} // namespace dc24h::admin
