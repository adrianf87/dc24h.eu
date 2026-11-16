#include "dc24h/users/session_manager.hpp"

#include <algorithm>
#include <stdexcept>

namespace dc24h::users {
Result Session::mark_authenticated_ready(accounts::AccountId account, TimePoint now, accounts::Role role) noexcept {
    if (accounts::role_key(role).empty()) { return SessionError::invalid_role; }
    if (const auto error = mark_ready(now)) { return error; }
    authentication_ = AuthSession{id_,account,transport_,now,role};
    return {};
}
MutationResult SessionManager::mark_authenticated_ready(SessionId id, accounts::AccountId account, TimePoint now, accounts::Role role) noexcept {
    return mutate(id,[=](Session& model) { return model.mark_authenticated_ready(account,now,role); });
}
std::optional<AuthSession> SessionManager::authenticated(SessionId id) const noexcept {
    const auto* model = find(id);
    return model && model->ready() ? model->authentication() : std::nullopt;
}
std::vector<AuthSession> SessionManager::authenticated_page(accounts::AccountId account,
    std::uint64_t after, std::size_t limit) const {
    if (limit == 0 || limit > max_page_size) { throw std::invalid_argument("Invalid authenticated session page size"); }
    std::vector<AuthSession> result;
    result.reserve(std::min(limit,size()));
    for (auto it = sessions_.upper_bound(after); it != sessions_.end() && result.size() < limit; ++it) {
        const auto auth = it->second->authentication();
        if (it->second->ready() && auth && auth->account_id == account) { result.push_back(*auth); }
    }
    return result;
}
} // namespace dc24h::users
