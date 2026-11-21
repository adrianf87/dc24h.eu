#include "service.hpp"

#include "dc24h/adc/protocol.hpp"
#include "dc24h/admin/crypto.hpp"
#include "dc24h/db/store.hpp"
#include "dc24h/net/server.hpp"
#include "dc24h/version.hpp"

#include <boost/beast/core/string.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <future>
#include <set>

namespace dc24h::admin {
namespace {

using Json = nlohmann::json;
using Status = http::status;
using Method = http::verb;
using namespace std::chrono_literals;

constexpr std::string_view cookie_name = "__Host-dc24h_session";
constexpr std::uint64_t session_lifetime = 8 * 60 * 60;
constexpr std::size_t max_page_size = 100;

struct ClientError {
    Status status;
    std::string_view code;
    std::string_view message;
};

constexpr ClientError invalid_request{Status::bad_request, "invalid_request", "Invalid request"};
constexpr ClientError invalid_json{Status::bad_request, "invalid_json", "Invalid JSON fields"};
constexpr ClientError unauthorized{Status::unauthorized, "unauthorized", "Authentication required"};
constexpr ClientError unavailable{Status::service_unavailable, "unavailable",
                                  "Service temporarily unavailable"};

std::string_view view(boost::beast::string_view text) { return {text.data(), text.size()}; }

std::string_view trim(std::string_view value) {
    const auto start = value.find_first_not_of(" \t");
    return start == value.npos ? std::string_view{}
                               : value.substr(start, value.find_last_not_of(" \t") - start + 1);
}

std::string_view header(const Request &request, boost::beast::string_view key) {
    return request.count(key) == 1 ? view(request[key]) : std::string_view{};
}

template <typename Integer> Integer positive_number(std::string_view input) {
    Integer number{};
    if (!input.empty()) {
        const auto end = input.data() + input.size();
        const auto result = std::from_chars(input.data(), end, number);
        if (result.ec == std::errc{} && result.ptr == end && number > 0) {
            return number;
        }
    }
    throw invalid_request;
}

std::string require_https_origin(std::string_view input) {
    constexpr std::string_view scheme = "https://";
    if (input.starts_with(scheme) && input.size() <= 270) {
        const auto authority = input.substr(scheme.size());
        const auto colon = authority.find(':');
        const auto host = authority.substr(0, colon);
        if (!host.empty() && !host.starts_with('.') && !host.ends_with('.') &&
            std::ranges::all_of(host, [](char byte) {
                return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
                       byte == '.' || byte == '-';
            })) {
            if (colon == authority.npos) {
                return std::string(input);
            }
            try {
                const auto port = authority.substr(colon + 1);
                if (!port.starts_with('0') && port != "443") {
                    static_cast<void>(positive_number<std::uint16_t>(port));
                    return std::string(input);
                }
            } catch (const ClientError &) {
            }
        }
    }
    throw std::invalid_argument("Admin API requires a canonical HTTPS origin");
}

void check_origin(const Request &request, std::string_view origin) {
    if (header(request, "Origin") != origin) {
        throw ClientError{Status::forbidden, "invalid_origin", "Request origin is not allowed"};
    }
}

Response json_response(Status status, const Json &json) {
    Response response{status, 11};
    response.set(http::field::cache_control, "no-store");
    response.set("X-Content-Type-Options", "nosniff");
    response.keep_alive(false);
    if (status != Status::no_content) {
        response.set(http::field::content_type, "application/json");
        response.body() = json.dump();
    }
    response.prepare_payload();
    return response;
}

Response error_response(const ClientError &error) {
    return admin::error_response(error.status, error.code, error.message);
}

std::string session_cookie(std::string_view token, std::uint64_t lifetime) {
    return std::string(cookie_name) + '=' + std::string(token) +
           "; Path=/; HttpOnly; Secure; SameSite=Strict; Max-Age=" + std::to_string(lifetime);
}

std::string_view session_token(const Request &request) {
    std::string_view token;
    for (const auto &field : request) {
        if (field.name() != http::field::cookie) {
            continue;
        }
        auto cookies = view(field.value());
        while (!cookies.empty()) {
            const auto end = cookies.find(';');
            const auto cookie = trim(cookies.substr(0, end));
            const auto equals = cookie.find('=');
            if (cookie.substr(0, equals) == cookie_name) {
                if (!token.empty() || equals == cookie.npos ||
                    !valid_token(cookie.substr(equals + 1))) {
                    throw unauthorized;
                }
                token = cookie.substr(equals + 1);
            }
            if (end == cookies.npos) {
                break;
            }
            cookies.remove_prefix(end + 1);
        }
    }
    if (token.empty()) {
        throw unauthorized;
    }
    return token;
}

Json require_object(const Request &request) {
    const auto type = header(request, "Content-Type");
    const auto semi = type.find(';');
    const auto media_type = trim(type.substr(0, semi));
    const auto parameter = semi == type.npos ? std::string_view{} : trim(type.substr(semi + 1));
    if (!boost::beast::iequals({media_type.data(), media_type.size()}, "application/json") ||
        (!parameter.empty() &&
         !boost::beast::iequals({parameter.data(), parameter.size()}, "charset=utf-8")) ||
        request.count(http::field::content_encoding) != 0) {
        throw ClientError{Status::unsupported_media_type, "unsupported_media_type",
                          "Expected application/json"};
    }
    if (request.body().find('\0') != std::string::npos) {
        throw invalid_json;
    }
    std::set<std::string> keys;
    const auto check = [&keys](int depth, Json::parse_event_t event, Json &value) {
        using Event = Json::parse_event_t;
        if ((event == Event::object_start && depth != 0) || event == Event::array_start ||
            (event == Event::key && !keys.insert(value.get<std::string>()).second)) {
            throw invalid_json;
        }
        return true;
    };
    auto value = Json::parse(request.body(), check, false);
    if (!value.is_object()) {
        throw invalid_json;
    }
    return value;
}

std::string_view text_field(const Json &object, const char *key) {
    const auto found = object.find(key);
    if (found == object.end() || !found->is_string()) {
        throw invalid_json;
    }
    return found->get_ref<const std::string &>();
}

Json account_json(const db::AdminAccount &account, std::string_view token) {
    return {{"username", account.username},
            {"role", db::role_name(account.role)},
            {"csrf_token", csrf_token(token)}};
}

struct Page {
    std::size_t limit = 50;
    std::string_view after;
};

Page parse_page(std::string_view query) {
    Page page;
    bool has_limit = false;
    while (!query.empty()) {
        const auto amp = query.find('&');
        const auto field = query.substr(0, amp);
        if (field.starts_with("limit=") && !has_limit) {
            page.limit = positive_number<std::size_t>(field.substr(6));
            if (page.limit > max_page_size) {
                throw invalid_request;
            }
            has_limit = true;
        } else if (field.starts_with("after=") && page.after.empty() && field.size() > 6) {
            page.after = field.substr(6);
        } else {
            throw invalid_request;
        }
        if (amp == query.npos) {
            break;
        }
        query.remove_prefix(amp + 1);
        if (query.empty()) {
            throw invalid_request;
        }
    }
    return page;
}

template <typename Row, typename Encode, typename Cursor>
Response page_response(const std::vector<Row> &rows, std::size_t limit, Encode encode,
                       Cursor cursor) {
    auto items = Json::array();
    for (std::size_t index = 0; index < std::min(rows.size(), limit); ++index) {
        items.push_back(encode(rows[index]));
    }
    const auto next = rows.size() > limit ? Json(cursor(rows[limit - 1])) : Json(nullptr);
    return json_response(Status::ok, {{"items", std::move(items)}, {"next_cursor", next}});
}

Json nullable(const std::optional<std::string> &value) {
    return value ? Json(*value) : Json(nullptr);
}

Json ban_json(const db::Ban &ban) {
    return {{"id", std::to_string(ban.id)},  {"kind", ban.kind == db::BanKind::cid ? "cid" : "ip"},
            {"identifier", ban.identifier},  {"reason", ban.reason},
            {"created_utc", ban.created_at}, {"expires_utc", nullable(ban.expires_at)},
            {"actor", nullable(ban.actor)}};
}

Json audit_json(const db::AuditEvent &event) {
    return {{"id", std::to_string(event.id)},         {"type", event.type},
            {"created_utc", event.created_at},        {"actor", nullable(event.actor)},
            {"object_id", nullable(event.object_id)}, {"details", event.details}};
}

Response users_page(net::Server &hub, Page page) {
    if (!page.after.empty() && !adc::is_sid(page.after)) {
        throw invalid_request;
    }
    auto reply = std::make_shared<std::promise<std::vector<hub::OnlineUser>>>();
    auto rows = reply->get_future();
    if (!hub.read_users(std::string(page.after), page.limit + 1,
                        [reply](auto users) { reply->set_value(std::move(users)); }) ||
        rows.wait_for(1s) != std::future_status::ready) {
        throw unavailable;
    }
    return page_response(
        rows.get(), page.limit,
        [](const hub::OnlineUser &user) -> Json {
            return {{"sid", user.sid}, {"nickname", user.nickname}, {"cid", user.cid}};
        },
        [](const hub::OnlineUser &user) { return user.sid; });
}

Response add_ban(db::Store &store, const Request &request, std::string_view actor) {
    const auto data = require_object(request);
    const auto kind = text_field(data, "kind");
    const auto identifier = text_field(data, "identifier");
    const auto reason = text_field(data, "reason");
    const auto expiry = data.find("duration_seconds");
    if (data.size() != (expiry == data.end() ? 3U : 4U) || (kind != "cid" && kind != "ip")) {
        throw invalid_json;
    }
    std::optional<std::uint32_t> duration;
    if (expiry != data.end() && !expiry->is_null()) {
        if (!expiry->is_number_unsigned() || *expiry == 0 ||
            expiry->get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max()) {
            throw invalid_json;
        }
        duration = expiry->get<std::uint32_t>();
    }
    const auto type = kind == "cid" ? db::BanKind::cid : db::BanKind::ip;
    const auto id = std::to_string(store.add_ban({type, identifier, reason, duration}, actor));
    return json_response(Status::created, {{"id", id}});
}

enum class Operation { login, logout, me, status, users, bans, ban, audit };

struct Route {
    Operation operation;
    Method method;
    std::string_view id{};
};

Route route(std::string_view path, Method method) {
    constexpr std::string_view prefix = "/api/v1/";
    if (path.starts_with(prefix)) {
        path.remove_prefix(prefix.size());
        if (path == "auth/login") {
            return {Operation::login, Method::post};
        }
        if (path == "auth/logout") {
            return {Operation::logout, Method::post};
        }
        if (path == "auth/me") {
            return {Operation::me, Method::get};
        }
        if (path == "status") {
            return {Operation::status, Method::get};
        }
        if (path == "users") {
            return {Operation::users, Method::get};
        }
        if (path == "bans") {
            return {Operation::bans, method == Method::post ? Method::post : Method::get};
        }
        if (path.starts_with("bans/")) {
            return {Operation::ban, Method::delete_, path.substr(5)};
        }
        if (path == "audit") {
            return {Operation::audit, Method::get};
        }
    }
    throw ClientError{Status::not_found, "not_found", "Endpoint not found"};
}

} // namespace

Response error_response(http::status status, std::string_view code, std::string_view message) {
    return json_response(status, {{"error", {{"code", code}, {"message", message}}}});
}

Service::Service(net::Server &hub, db::Config database, std::string origin)
    : hub_(hub), database_(std::move(database)), origin_(require_https_origin(origin)),
      dummy_hash_(hash_password(random_token())), login_window_(std::chrono::steady_clock::now()) {}

bool Service::allow_login(std::string_view username) {
    const std::lock_guard lock(login_mutex_);
    const auto now = std::chrono::steady_clock::now();
    if (now - login_window_ >= 1min) {
        login_window_ = now;
        attempts_.clear();
        total_attempts_ = 0;
    }
    if (total_attempts_ >= 30) {
        return false;
    }
    auto &attempts = attempts_[std::string(username)];
    if (attempts >= 5) {
        return false;
    }
    ++attempts;
    ++total_attempts_;
    return true;
}

Response Service::login(const Request &request) {
    check_origin(request, origin_);
    const auto data = require_object(request);
    const auto username = text_field(data, "username");
    const auto password = text_field(data, "password");
    if (data.size() != 2 || !db::valid_admin_username(username) || !valid_password(password)) {
        throw invalid_json;
    }
    if (!allow_login(username)) {
        auto response = error_response(Status::too_many_requests, "login_rate_limit",
                                       "Too many login attempts");
        response.set(http::field::retry_after, "60");
        return response;
    }
    db::Store store(database_);
    store.require_current_schema();
    const auto candidate = store.admin_credentials(username);
    if (!verify_password(password, candidate ? candidate->password_hash : dummy_hash_) ||
        !candidate) {
        return error_response(Status::unauthorized, "invalid_credentials",
                              "Invalid username or password");
    }
    const auto token = random_token();
    store.create_admin_session(candidate->account, session_digest(token), session_lifetime);
    auto response = json_response(Status::ok, account_json(candidate->account, token));
    response.set(http::field::set_cookie, session_cookie(token, session_lifetime));
    return response;
}

Response Service::dispatch(const Request &request) {
    const auto target = view(request.target());
    const auto question = target.find('?');
    const auto endpoint = route(target.substr(0, question), request.method());
    if (endpoint.method != request.method()) {
        auto response = error_response(Status::method_not_allowed, "method_not_allowed",
                                       "Method is not allowed for this endpoint");
        response.set(http::field::allow, endpoint.operation == Operation::bans
                                             ? "GET, POST"
                                             : http::to_string(endpoint.method));
        return response;
    }
    const bool writing_ban =
        (endpoint.operation == Operation::bans && endpoint.method == Method::post) ||
        endpoint.operation == Operation::ban;
    const bool list = endpoint.operation == Operation::users ||
                      endpoint.operation == Operation::audit ||
                      (endpoint.operation == Operation::bans && endpoint.method == Method::get);
    if ((!list && question != target.npos) ||
        (endpoint.operation != Operation::login && endpoint.method != Method::post &&
         !request.body().empty()) ||
        (endpoint.operation == Operation::logout && !request.body().empty())) {
        throw invalid_request;
    }
    if (endpoint.operation == Operation::login) {
        return login(request);
    }
    const auto token = session_token(request);
    const auto digest = session_digest(token);
    db::Store store(database_);
    store.require_current_schema();
    const auto account = store.admin_session(digest);
    if (!account) {
        throw unauthorized;
    }
    if (writing_ban && account->role != db::AdminRole::admin) {
        throw ClientError{Status::forbidden, "forbidden", "Administrator role is required"};
    }
    if (endpoint.method != Method::get) {
        check_origin(request, origin_);
        if (!tokens_match(header(request, "X-CSRF-Token"), csrf_token(token))) {
            throw ClientError{Status::forbidden, "invalid_csrf", "CSRF token is invalid"};
        }
    }
    const auto page = question == target.npos ? Page{} : parse_page(target.substr(question + 1));
    switch (endpoint.operation) {
    case Operation::logout: {
        store.revoke_admin_session(*account, digest);
        auto response = json_response(Status::no_content, nullptr);
        response.set(http::field::set_cookie, session_cookie({}, 0));
        return response;
    }
    case Operation::me:
        return json_response(Status::ok, account_json(*account, token));
    case Operation::status:
        return json_response(Status::ok, {{"version", version},
                                          {"active_connections", hub_.active_clients()},
                                          {"online_users", hub_.online_users()}});
    case Operation::users:
        return users_page(hub_, page);
    case Operation::bans:
        if (writing_ban) {
            return add_ban(store, request, account->username);
        }
        return page_response(
            store.bans(page.after.empty() ? 0 : positive_number<std::uint64_t>(page.after),
                       page.limit + 1),
            page.limit, ban_json, [](const db::Ban &ban) { return std::to_string(ban.id); });
    case Operation::ban:
        if (!store.remove_ban(positive_number<std::uint64_t>(endpoint.id), account->username)) {
            throw ClientError{Status::not_found, "not_found", "Ban not found"};
        }
        return json_response(Status::no_content, nullptr);
    case Operation::audit:
        return page_response(
            store.audit_events(page.after.empty() ? 0 : positive_number<std::uint64_t>(page.after),
                               page.limit + 1),
            page.limit, audit_json,
            [](const db::AuditEvent &event) { return std::to_string(event.id); });
    case Operation::login:
        break;
    }
    throw std::logic_error("Unrecognized administrator operation");
}

Response Service::respond(const Request &request) {
    try {
        return dispatch(request);
    } catch (const ClientError &error) {
        return error_response(error);
    } catch (const db::Error &) {
        return error_response(unavailable);
    } catch (const std::invalid_argument &) {
        return error_response(invalid_request);
    } catch (const std::exception &) {
        return error_response(Status::internal_server_error, "internal_error",
                              "Internal server error");
    }
}

} // namespace dc24h::admin
