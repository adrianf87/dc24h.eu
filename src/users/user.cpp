#include "dc24h/users/user.hpp"

#include <algorithm>
#include <utility>

namespace dc24h::users {
namespace {
bool bounded(std::string_view value, std::size_t maximum) noexcept {
    return !value.empty() && value.size() <= maximum && value.find('\0') == std::string_view::npos;
}
bool public_text(std::string_view text, std::size_t maximum) noexcept {
    return text.size() <= maximum && std::none_of(text.begin(),text.end(),[](unsigned char ch) { return ch < 32 || ch == 127; });
}
bool feature(std::string_view name) noexcept {
    return name.size()==4 && name[0]>='A' && name[0]<='Z' &&
        std::all_of(name.begin()+1,name.end(),[](char ch) { return (ch>='A' && ch<='Z') || (ch>='0' && ch<='9'); });
}
}
Identity::Identity(std::string_view sid, std::string_view cid, std::string_view nickname)
    : sid_(sid), cid_(cid), nickname_(nickname) {}
std::optional<Identity> Identity::create(std::string_view sid, std::string_view cid, std::string_view nickname) {
    if (!bounded(sid,max_session_key_bytes) || !bounded(cid,max_client_key_bytes) || !bounded(nickname,max_nickname_bytes)) { return std::nullopt; }
    return Identity(sid,cid,nickname);
}
Profile::Profile(ProfileData data) : data_(std::move(data)) {}
std::optional<Profile> Profile::create(ProfileData data) {
    if (!public_text(data.description,2048) || !public_text(data.email,254) ||
        !public_text(data.application,128) || !public_text(data.version,128) || !public_text(data.locale,64) ||
        data.features.size()>max_features) { return std::nullopt; }
    for (const auto number : {data.shared_bytes,data.shared_files,data.slots}) {
        if (number && *number<0) { return std::nullopt; }
    }
    auto bytes=data.description.size()+data.email.size()+data.application.size()+data.version.size()+data.locale.size();
    for (std::size_t i=0;i<data.features.size();++i) {
        if (!feature(data.features[i]) || std::find(data.features.begin(),data.features.begin()+static_cast<std::ptrdiff_t>(i),data.features[i])!=data.features.begin()+static_cast<std::ptrdiff_t>(i)) { return std::nullopt; }
        bytes+=data.features[i].size();
    }
    if (bytes>max_bytes) { return std::nullopt; }
    return Profile(std::move(data));
}
User::User(Identity identity, Profile profile) : identity_(std::move(identity)), profile_(std::move(profile)) {}
bool User::valid() const noexcept {
    return bounded(identity_.sid(),max_session_key_bytes) && bounded(identity_.cid(),max_client_key_bytes) && bounded(identity_.nickname(),max_nickname_bytes);
}
std::optional<User> User::create(Identity identity, Profile profile) {
    User result(std::move(identity),std::move(profile));
    if (!result.valid()) { return std::nullopt; }
    return result;
}
} // namespace dc24h::users
