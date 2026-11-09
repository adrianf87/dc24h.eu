#include "dc24h/api/http.hpp"

#include <openssl/crypto.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <unordered_set>

namespace dc24h::api {
namespace {
bool tchar(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        std::string_view("!#$%&'*+-.^_`|~").find(static_cast<char>(c)) != std::string_view::npos;
}
std::string lower(std::string_view text) {
    std::string result(text);
    for (auto& c : result) { if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c + ('a' - 'A')); } }
    return result;
}
std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) { value.remove_prefix(1); }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) { value.remove_suffix(1); }
    return value;
}
} // namespace

ParseResult parse_request(std::string_view bytes) {
    if (bytes.size() > max_request_bytes) { return {ParseState::too_large, {}}; }
    // Reject bare CR/LF as soon as enough bytes exist to classify them.
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c == '\n' && (i == 0 || bytes[i - 1] != '\r')) { return {ParseState::invalid, {}}; }
        if (c == '\r' && i + 1 < bytes.size() && bytes[i + 1] != '\n') { return {ParseState::invalid, {}}; }
        if ((c < 32 && c != '\r' && c != '\n' && c != '\t') || c > 126) { return {ParseState::invalid, {}}; }
    }
    const auto end = bytes.find("\r\n\r\n");
    if (end == std::string_view::npos) {
        return {bytes.size() == max_request_bytes ? ParseState::too_large : ParseState::incomplete, {}};
    }
    if (end + 4 != bytes.size()) { return {ParseState::invalid, {}}; }
    const auto first_end = bytes.find("\r\n");
    const auto line = bytes.substr(0, first_end);
    if (line.size() > 1024) { return {ParseState::invalid, {}}; }
    const auto space = line.find(' ');
    const auto last = line.rfind(' ');
    if (space == std::string_view::npos || last == space || line.substr(last + 1) != "HTTP/1.1") {
        return {ParseState::invalid, {}};
    }
    Request request;
    request.method = line.substr(0, space);
    request.target = line.substr(space + 1, last - space - 1);
    if (request.method.empty() || !std::all_of(request.method.begin(), request.method.end(), tchar) ||
        request.target.empty() || request.target.front() != '/' || request.target.starts_with("//") ||
        request.target.find_first_of(" \t#") != std::string::npos) { return {ParseState::invalid, {}}; }
    std::unordered_set<std::string> headers;
    std::size_t offset = first_end + 2;
    bool host = false;
    while (offset < end + 2) {
        const auto next = bytes.find("\r\n", offset);
        const auto header = bytes.substr(offset, next - offset);
        const auto colon = header.find(':');
        const auto name = header.substr(0, colon);
        if (colon == std::string_view::npos || name.empty() || !std::all_of(name.begin(), name.end(), tchar)) {
            return {ParseState::invalid, {}};
        }
        const auto key = lower(name);
        if (!headers.insert(key).second || headers.size() > max_header_count) { return {ParseState::invalid, {}}; }
        const auto value = trim(header.substr(colon + 1));
        if (std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c > 126; }) ||
            key == "transfer-encoding" || key == "expect" || key == "upgrade" ||
            (key == "content-length" && value != "0")) { return {ParseState::invalid, {}}; }
        if (key == "host") {
            if (value.empty() || value.find_first_of(" ,/\\@") != std::string_view::npos) { return {ParseState::invalid, {}}; }
            host = true;
        }
        if (key == "authorization") { request.authorization = value; }
        offset = next + 2;
    }
    if (!host) { return {ParseState::invalid, {}}; }
    return {ParseState::complete, std::move(request)};
}

bool valid_token(std::string_view token) noexcept {
    return token.size() == 64 && std::all_of(token.begin(), token.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}
bool authenticate(std::string_view authorization, std::string_view token) noexcept {
    if (token.size() != 64 || authorization.size() != 71 || authorization[6] != ' ') { return false; }
    constexpr std::string_view scheme = "bearer";
    for (std::size_t i = 0; i < scheme.size(); ++i) {
        auto c = authorization[i];
        if (c >= 'A' && c <= 'Z') { c = static_cast<char>(c + ('a' - 'A')); }
        if (c != scheme[i]) { return false; }
    }
    return CRYPTO_memcmp(authorization.data() + 7, token.data(), 64) == 0;
}
std::string response(unsigned int status, std::string_view json, bool head) {
    const auto reason = status == 200 ? "OK" : status == 401 ? "Unauthorized" :
        status == 404 ? "Not Found" : status == 405 ? "Method Not Allowed" :
        status == 431 ? "Request Header Fields Too Large" : "Bad Request";
    std::string result = "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
    const auto now = std::time(nullptr);
    std::tm utc{};
    char date[64]{};
    // Use numeric time fields and fixed English names regardless of process locale.
    if (::gmtime_r(&now, &utc) != nullptr) {
        constexpr const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        constexpr const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        std::snprintf(date, sizeof(date), "%s, %02d %s %04d %02d:%02d:%02d GMT",
            days[utc.tm_wday], utc.tm_mday, months[utc.tm_mon], utc.tm_year + 1900,
            utc.tm_hour, utc.tm_min, utc.tm_sec);
        result += std::string("Date: ") + date + "\r\n";
    }
    result += "Content-Type: application/json; charset=utf-8\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
    if (status == 401) { result += "WWW-Authenticate: Bearer realm=\"dc24h-admin\"\r\n"; }
    if (status == 405) { result += "Allow: GET\r\n"; }
    result += "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n";
    if (!head) { result += json; }
    return result;
}
} // namespace dc24h::api
