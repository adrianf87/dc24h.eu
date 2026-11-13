#include "dc24h/users/user.hpp"
#include "dc24h/text/public_text.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <utility>

namespace dc24h::users {
namespace {
bool bounded(std::string_view value, std::size_t maximum) noexcept {
    return !value.empty() && value.size() <= maximum && value.find('\0') == std::string_view::npos;
}
bool public_text(std::string_view value, std::size_t maximum) {
    return value.size() <= maximum && text::valid_public_text(value);
}
bool numeric_address(const std::string& value, int family) noexcept {
    if (value.empty()) { return true; }
    if (value.size() >= INET6_ADDRSTRLEN || value.find('\0') != std::string::npos) { return false; }
    std::array<unsigned char,16> address{};
    return inet_pton(family,value.c_str(),address.data()) == 1;
}
bool feature(std::string_view name) noexcept {
    return name.size()==4 && name[0]>='A' && name[0]<='Z' &&
        std::all_of(name.begin()+1,name.end(),[](char ch) { return (ch>='A' && ch<='Z') || (ch>='0' && ch<='9'); });
}
}
std::optional<Nickname> Nickname::create(std::string_view value) {
    if (!bounded(value,max_nickname_bytes) || !text::valid_public_text(value,true)) { return std::nullopt; }
    return Nickname(value);
}
Identity::Identity(std::string_view sid, std::string_view cid, Nickname nickname)
    : sid_(sid), cid_(cid), nickname_(std::move(nickname)) {}
std::optional<Identity> Identity::create(std::string_view sid, std::string_view cid, std::string_view nickname) {
    if (!bounded(sid,max_session_key_bytes) || !bounded(cid,max_client_key_bytes)) { return std::nullopt; }
    auto checked=Nickname::create(nickname);
    if (!checked) { return std::nullopt; }
    return Identity(sid,cid,std::move(*checked));
}
std::optional<Identity> Identity::create(std::string_view sid, std::string_view cid, Nickname nickname) {
    if (!bounded(sid,max_session_key_bytes) || !bounded(cid,max_client_key_bytes) || !nickname.valid()) { return std::nullopt; }
    return Identity(sid,cid,std::move(nickname));
}
Profile::Profile(ProfileData data) : data_(std::move(data)) {}
std::optional<Profile> Profile::create(ProfileData data) {
    if (!public_text(data.description,2048) || !public_text(data.email,254) ||
        !public_text(data.application,128) || !public_text(data.version,128) || !public_text(data.locale,64) ||
        data.features.size()>max_features) { return std::nullopt; }
    for (const auto number : {data.shared_bytes,data.shared_files,data.slots}) {
        if (number && *number<0) { return std::nullopt; }
    }
    if (!numeric_address(data.ipv4,AF_INET) || !numeric_address(data.ipv6,AF_INET6)) { return std::nullopt; }
    for (const auto port : {data.udp4_port,data.udp6_port}) {
        if (port && (*port < 0 || *port > 65535)) { return std::nullopt; }
    }
    auto bytes=data.ipv4.size()+data.ipv6.size()+data.description.size()+data.email.size()+data.application.size()+data.version.size()+data.locale.size();
    for (std::size_t i=0;i<data.features.size();++i) {
        if (!feature(data.features[i]) || std::find(data.features.begin(),data.features.begin()+static_cast<std::ptrdiff_t>(i),data.features[i])!=data.features.begin()+static_cast<std::ptrdiff_t>(i)) { return std::nullopt; }
        bytes+=data.features[i].size();
    }
    if (bytes>max_bytes) { return std::nullopt; }
    return Profile(std::move(data));
}
User::User(Identity identity, Profile profile) : identity_(std::move(identity)), profile_(std::move(profile)) {}
bool User::valid() const noexcept {
    return bounded(identity_.sid(),max_session_key_bytes) && bounded(identity_.cid(),max_client_key_bytes) && identity_.nickname_value().valid();
}
std::optional<User> User::with_nickname(Nickname nickname) const {
    if (!valid()) { return std::nullopt; }
    auto identity=Identity::create(identity_.sid(),identity_.cid(),std::move(nickname));
    if (!identity) { return std::nullopt; }
    return create(std::move(*identity),profile_);
}
std::optional<User> User::create(Identity identity, Profile profile) {
    User result(std::move(identity),std::move(profile));
    if (!result.valid()) { return std::nullopt; }
    return result;
}
} // namespace dc24h::users
