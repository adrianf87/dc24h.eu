#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dc24h::adc {

enum class MessageType : char {
    broadcast = 'B',
    client = 'C',
    direct = 'D',
    echo = 'E',
    feature = 'F',
    hub = 'H',
    information = 'I',
    udp = 'U',
};

struct FeatureSelector {
    std::string name;
    bool required;
};

struct Message {
    MessageType type{};
    std::string command;
    std::optional<std::string> sender_sid;
    std::optional<std::string> target_sid;
    std::optional<std::string> sender_cid;
    std::vector<FeatureSelector> selectors;
    std::vector<std::string> parameters;

    [[nodiscard]] std::string fourcc() const;
};

[[nodiscard]] bool is_sid(std::string_view value) noexcept;
[[nodiscard]] bool is_cid(std::string_view value) noexcept; // TIGR/192
[[nodiscard]] bool is_feature(std::string_view value) noexcept;

enum class IdentityCheck { valid, invalid_cid, invalid_pid, mismatch, hash_unavailable };

[[nodiscard]] IdentityCheck verify_tiger_identity(std::string_view cid, std::string_view pid);

// These validation results borrow their text from the source Message.
struct FeatureChange {
    std::string_view name;
    bool added;
};

struct Field {
    std::string_view name;
    std::string_view value;
};

enum class Severity { success, recoverable, fatal };

struct Status {
    Severity severity;
    unsigned code;
    std::string_view description;
    std::vector<Field> fields;
};

struct Quit {
    std::string_view sid;
    std::vector<Field> fields;
};

struct Chat {
    std::string_view text;
    std::optional<std::string_view> private_group;
    bool third_person = false;
};

[[nodiscard]] std::string format_status(std::string_view code, std::string_view text,
                                        std::string_view field = {});
[[nodiscard]] std::optional<std::vector<FeatureChange>> parse_support(const Message &message);
[[nodiscard]] std::optional<std::string_view> parse_sid(const Message &message);
[[nodiscard]] std::optional<std::vector<Field>> parse_info(const Message &message);
[[nodiscard]] std::optional<Status> parse_status(const Message &message);
[[nodiscard]] std::optional<Quit> parse_quit(const Message &message);
[[nodiscard]] std::optional<Chat> parse_chat(const Message &message);
[[nodiscard]] std::optional<std::string_view> parse_password_challenge(const Message &message);
[[nodiscard]] std::optional<std::string_view> parse_password_response(const Message &message);

} // namespace dc24h::adc
