#include "dc24h/adc/user_list.hpp"
#include "dc24h/adc/serializer.hpp"

namespace dc24h::adc {
namespace {
bool verified(const ClientInfo& info) noexcept {
    const auto identity = info.identity();
    const auto cid = info.field("ID");
    return identity && cid && TigerCid::parse(*cid) == identity->cid() && info.field("NI");
}
}
UserListResult UserList::prepare(users::SessionId joining, const ClientSession& client,
        const InfoLookup& lookup, std::size_t budget) const {
    const auto* model = sessions_.find(joining);
    if (!model || model->phase() != users::Phase::negotiating || client.state() != ConnectionState::normal) {
        return UserListError::invalid_state;
    }
    const auto own_sid = Sid::parse(client.sid());
    if (!own_sid || own_sid->value() == 0 || !client.info() || !verified(*client.info())) {
        return UserListError::invalid_identity;
    }
    if (sessions_.by_sid(client.sid()) || sessions_.by_cid(*client.info()->field("ID")) ||
        sessions_.by_nickname(*client.info()->field("NI"))) { return UserListError::invalid_identity; }
    std::vector<Message> frames;
    std::size_t bytes = 0;
    const auto append = [&](Message message) {
        const auto encoded = serialize(message);
        const auto* wire = std::get_if<std::string>(&encoded);
        if (!wire || wire->size() > budget - bytes) { return false; }
        bytes += wire->size();
        frames.push_back(std::move(message));
        return true;
    };
    if (!append({MessageType::info, "INF", Command::inf, {}, {}, {},
            {"CT32", "NIdc24h.eu", "DEGuest login alpha"}, {}})) { return UserListError::output_limit; }
    std::uint64_t after = 0;
    for (;;) {
        const auto page = sessions_.public_users(after, users::SessionManager::max_page_size);
        if (page.empty()) { break; }
        for (const auto id : page) {
            const auto* existing = sessions_.find(id);
            const auto* info = lookup ? lookup(id) : nullptr;
            if (!existing || !existing->identity() || !info || !verified(*info) ||
                info->field("ID") != existing->identity()->cid() ||
                info->field("NI") != existing->identity()->nickname()) { return UserListError::inconsistent_source; }
            const auto sid = Sid::parse(existing->identity()->sid());
            if (!sid || sid->value() == 0) { return UserListError::inconsistent_source; }
            if (!append(info->public_message(*sid))) { return UserListError::output_limit; }
        }
        after = page.back().value();
    }
    if (!append(client.info()->public_message(*own_sid))) { return UserListError::output_limit; }
    return InitialUserList(std::move(frames), bytes);
}
} // namespace dc24h::adc
