#pragma once

#include <string>
#include <string_view>

namespace dc24h::admin {

[[nodiscard]] std::string hash_password(std::string_view password);
[[nodiscard]] bool verify_password(std::string_view password, const std::string &hash);
[[nodiscard]] bool valid_password(std::string_view password) noexcept;

[[nodiscard]] std::string random_token();
[[nodiscard]] bool valid_token(std::string_view token) noexcept;
[[nodiscard]] std::string session_digest(std::string_view token);
[[nodiscard]] std::string csrf_token(std::string_view token);
[[nodiscard]] bool tokens_match(std::string_view offered, std::string_view expected) noexcept;

} // namespace dc24h::admin
