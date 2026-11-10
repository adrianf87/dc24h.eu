#include "dc24h/adc/user.hpp"

#include <utility>

namespace dc24h::adc {
std::optional<users::User> user_from_info(const ClientInfo& info, Sid sid) {
    const auto verified=info.identity();
    const auto nickname=info.field("NI");
    if (sid.value()==0 || !verified || !nickname) { return std::nullopt; }
    auto identity=users::Identity::create(sid.str(),verified->cid().text(),*nickname);
    users::ProfileData data;
    data.description=info.field("DE").value_or("");
    data.email=info.field("EM").value_or("");
    data.application=info.field("AP").value_or("");
    data.version=info.field("VE").value_or("");
    data.locale=info.field("LO").value_or("");
    data.shared_bytes=info.integer("SS");
    data.shared_files=info.integer("SF");
    data.slots=info.integer("SL");
    auto features=info.field("SU").value_or("");
    while (!features.empty()) {
        const auto comma=features.find(',');
        data.features.emplace_back(features.substr(0,comma));
        if (comma==std::string_view::npos) { break; }
        features.remove_prefix(comma+1);
    }
    auto profile=users::Profile::create(std::move(data));
    if (!identity || !profile) { return std::nullopt; }
    return users::User::create(std::move(*identity),std::move(*profile));
}
} // namespace dc24h::adc
