#pragma once

#include "dc24h/adc/parser.hpp"
#include "dc24h/adc/sid.hpp"

#include <array>

namespace dc24h::adc {
enum class Severity { success, recoverable, fatal };
class StatusCode final {
public:
    // ADC xyy: severity 0/1/2; success is 000. Unknown error numbers remain valid.
    [[nodiscard]] static std::optional<StatusCode> parse(std::string_view value) noexcept;
    [[nodiscard]] Severity severity() const noexcept { return severity_; }
    [[nodiscard]] unsigned error() const noexcept { return error_; }
    [[nodiscard]] std::string str() const;
    friend bool operator==(StatusCode, StatusCode) = default;
private:
    StatusCode(Severity severity, unsigned error) noexcept : severity_(severity), error_(error) {}
    Severity severity_;
    unsigned error_;
};

enum class StatusError { invalid_message, invalid_code, field_limit, duplicate_field, invalid_field };
inline constexpr std::size_t max_status_text_bytes = 2048;
struct StatusView { StatusCode code; std::string_view description; };
struct QuitView {
    Sid sid;
    std::optional<Sid> initiator;
    std::optional<std::int64_t> reconnect_after;
    std::optional<std::string_view> message;
    std::optional<std::string_view> redirect;
    bool unwanted{false};
};
// Views borrow Message storage. Validate syntax/text/encoded bounds as well as
// known-field semantics. No state change, routing, redirect or ban side effects.
[[nodiscard]] std::variant<StatusView, StatusError> validate_status(const Message& message);
[[nodiscard]] std::variant<QuitView, StatusError> validate_quit(const Message& message);
// Public STA relay drops unknown fields (including PD/PAS); known values remain
// client metadata. Call only after successful validation and source/state gates.
[[nodiscard]] Message public_status(const Message& message);
// Builders throw invalid_argument for invalid local options; never truncate.
[[nodiscard]] Message make_status(StatusCode code, std::string_view description,
                                 std::optional<std::string_view> command = {});
struct QuitOptions {
    std::optional<Sid> initiator;
    std::optional<std::int64_t> reconnect_after;
    std::string_view message;
    std::string_view redirect;
    bool unwanted{false};
};
[[nodiscard]] Message make_quit(Sid sid, QuitOptions options = {});
} // namespace dc24h::adc
