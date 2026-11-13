#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dc24h::users {
inline constexpr std::size_t max_session_key_bytes = 64;
inline constexpr std::size_t max_client_key_bytes = 128;
inline constexpr std::size_t max_nickname_bytes = 256;

// An owning, checked public name. NFC is required, never silently rewritten.
// This generic model's 256-byte bound is wider than ADC's 64-byte nick policy.
class Nickname final {
public:
    [[nodiscard]] static std::optional<Nickname> create(std::string_view text);
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] bool valid() const noexcept { return !text_.empty() && text_.size()<=max_nickname_bytes; }
    friend bool operator==(const Nickname&, const Nickname&) = default;
private:
    explicit Nickname(std::string_view text) : text_(text) {}
    std::string text_;
};

// Bounded public keys supplied by the validating protocol adapter. These are
// neither account identity nor cryptographic/uniqueness validation.
class Identity final {
public:
    [[nodiscard]] static std::optional<Identity> create(std::string_view sid,
        std::string_view cid, std::string_view nickname);
    [[nodiscard]] static std::optional<Identity> create(std::string_view sid,
        std::string_view cid, Nickname nickname);
    [[nodiscard]] const std::string& sid() const noexcept { return sid_; }
    [[nodiscard]] const std::string& cid() const noexcept { return cid_; }
    [[nodiscard]] const std::string& nickname() const noexcept { return nickname_.text(); }
    [[nodiscard]] const Nickname& nickname_value() const noexcept { return nickname_; }
    friend bool operator==(const Identity&, const Identity&) = default;
private:
    Identity(std::string_view sid, std::string_view cid, Nickname nickname);
    std::string sid_, cid_;
    Nickname nickname_;
};

// Explicit public projection, not a map of arbitrary INF fields. Empty text and
// absent numbers mean no declaration; share/slot claims are not trusted facts.
struct ProfileData {
    std::string description, email, application, version, locale;
    std::optional<std::int64_t> shared_bytes, shared_files, slots;
    std::vector<std::string> features;
    // Numeric public endpoint declarations, checked by the protocol adapter
    // against the kernel peer address. Zero UDP ports explicitly disable UDP.
    std::string ipv4, ipv6;
    std::optional<std::int64_t> udp4_port, udp6_port;
    friend bool operator==(const ProfileData&, const ProfileData&) = default;
};
class Profile final {
public:
    static constexpr std::size_t max_bytes = 4096;
    static constexpr std::size_t max_features = 128;
    Profile() = default;
    [[nodiscard]] static std::optional<Profile> create(ProfileData data);
    [[nodiscard]] const ProfileData& data() const noexcept { return data_; }
    friend bool operator==(const Profile&, const Profile&) = default;
private:
    explicit Profile(ProfileData data);
    ProfileData data_;
};

// Owning value for one public guest on one connection, not a persistent account.
// Only Session can mutate a live user's identity/profile through lifecycle gates.
class User final {
public:
    [[nodiscard]] static std::optional<User> create(Identity identity, Profile profile = {});
    [[nodiscard]] const Identity& identity() const noexcept { return identity_; }
    [[nodiscard]] const Profile& profile() const noexcept { return profile_; }
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::optional<User> with_nickname(Nickname nickname) const;
    friend bool operator==(const User&, const User&) = default;
private:
    User(Identity identity, Profile profile);
    Identity identity_;
    Profile profile_;
};
} // namespace dc24h::users
