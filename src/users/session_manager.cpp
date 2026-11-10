#include "dc24h/users/session_manager.hpp"

#include <algorithm>
#include <stdexcept>

namespace dc24h::users {
SessionManager::SessionManager(std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0 || capacity > max_capacity) { throw std::invalid_argument("Invalid session registry capacity"); }
}
Registration SessionManager::add(SessionId id, Transport transport, TimePoint accepted) {
    if (id.value() <= greatest_id_) { return RegistryError::stale_id; }
    if (size() == capacity_) { return RegistryError::capacity_reached; }
    auto model = std::make_unique<Session>(id, transport, accepted);
    auto* result = model.get();
    sessions_.emplace(id.value(), std::move(model));
    greatest_id_ = id.value();
    return result;
}
Session* SessionManager::find(SessionId id) noexcept {
    const auto found = sessions_.find(id.value());
    return found == sessions_.end() ? nullptr : found->second.get();
}
const Session* SessionManager::find(SessionId id) const noexcept {
    const auto found = sessions_.find(id.value());
    return found == sessions_.end() ? nullptr : found->second.get();
}
std::optional<RegistryError> SessionManager::remove(SessionId id) noexcept {
    const auto found = sessions_.find(id.value());
    if (found == sessions_.end()) { return RegistryError::missing; }
    if (found->second->phase() != Phase::closed) { return RegistryError::not_closed; }
    sessions_.erase(found);
    return {};
}
std::optional<Snapshot> SessionManager::snapshot(SessionId id) const {
    const auto* model = find(id);
    return model ? std::optional<Snapshot>{model->snapshot()} : std::nullopt;
}
std::vector<Snapshot> SessionManager::page(std::uint64_t after, std::size_t limit) const {
    if (limit == 0 || limit > max_page_size) { throw std::invalid_argument("Invalid session snapshot page size"); }
    std::vector<Snapshot> result;
    result.reserve(std::min(limit, size()));
    for (auto it = sessions_.upper_bound(after); it != sessions_.end() && result.size() < limit; ++it) {
        result.push_back(it->second->snapshot());
    }
    return result;
}
SessionCounts SessionManager::counts() const noexcept {
    SessionCounts result;
    result.total = size();
    for (const auto& [id, model] : sessions_) {
        (void)id;
        switch (model->phase()) {
        case Phase::accepted: ++result.accepted; break;
        case Phase::negotiating: ++result.negotiating; break;
        case Phase::identified: ++result.identified; break;
        case Phase::ready: ++result.ready; break;
        case Phase::closing: ++result.closing; break;
        case Phase::closed: ++result.closed; break;
        }
    }
    return result;
}
} // namespace dc24h::users
