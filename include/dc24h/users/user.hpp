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

// Bounded public keys supplied by the validating protocol adapter. These are
// neither account identity nor cryptographic/uniqueness validation.
class Identity final {
public:
    [[nodiscard]] static std::optional<Identity> create(std::string_view sid,
        std::string_view cid, std::string_view nickname);
    [[nodiscard]] const std::string& sid() const noexcept { return sid_; }
    [[nodiscard]] const std::string& cid() const noexcept { return cid_; }
    [[nodiscard]] const std::string& nickname() const noexcept { return nickname_; }
    friend bool operator==(const Identity&, const Identity&) = default;
private:
    friend class Session;
    Identity(std::string_view sid, std::string_view cid, std::string_view nickname);
    std::string sid_, cid_, nickname_;
};

// Explicit public projection, not a map of arbitrary INF fields. Empty text and
// absent numbers mean no declaration; share/slot claims are not trusted facts.
struct ProfileData {
    std::string description, email, application, version, locale;
    std::optional<std::int64_t> shared_bytes, shared_files, slots;
    std::vector<std::string> features;
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
    friend bool operator==(const User&, const User&) = default;
private:
    friend class Session;
    User(Identity identity, Profile profile);
    Identity identity_;
    Profile profile_;
};
} // namespace dc24h::users
