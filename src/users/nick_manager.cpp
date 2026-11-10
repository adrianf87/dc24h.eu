#include "dc24h/users/nick_manager.hpp"

#include <stdexcept>

namespace dc24h::users {
NickManager::NickManager(std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0 || capacity > max_capacity) {
        throw std::invalid_argument("Invalid nickname registry capacity");
    }
}
std::optional<SessionId> NickManager::owner(std::string_view nickname) const noexcept {
    if (nickname.empty() || nickname.size() > max_nickname_bytes ||
        nickname.find('\0') != std::string_view::npos) { return std::nullopt; }
    const auto found = names_.find(nickname);
    return found == names_.end() ? std::nullopt : std::optional<SessionId>{found->second};
}
NickResult NickManager::reserve(SessionId id, std::string_view nickname) {
    const auto checked = Nickname::create(nickname);
    return checked ? reserve(id, *checked) : NickResult{NickError::invalid_nickname};
}
NickResult NickManager::reserve(SessionId id, const Nickname& nickname) {
    if (!nickname.valid()) { return NickError::invalid_nickname; }
    if (owners_.contains(id.value())) { return NickError::owner_exists; }
    if (owner(nickname.text())) { return NickError::duplicate_nickname; }
    if (size() == capacity_) { return NickError::capacity_reached; }
    const auto added = names_.emplace(nickname.text(), id).first;
    try {
        owners_.emplace(id.value(), added);
    } catch (...) {
        names_.erase(added);
        throw;
    }
    return {};
}
NickResult NickManager::rename(SessionId id, std::string_view nickname) {
    const auto checked = Nickname::create(nickname);
    return checked ? rename(id, *checked) : NickResult{NickError::invalid_nickname};
}
NickResult NickManager::rename(SessionId id, const Nickname& nickname) {
    if (!nickname.valid()) { return NickError::invalid_nickname; }
    const auto current = owners_.find(id.value());
    if (current == owners_.end()) { return NickError::missing; }
    if (current->second->first == nickname.text()) { return {}; }
    if (owner(nickname.text())) { return NickError::duplicate_nickname; }
    // Allocate before releasing the old name; iterator assignment/erase cannot throw.
    const auto replacement = names_.emplace(nickname.text(), id).first;
    const auto previous = current->second;
    current->second = replacement;
    names_.erase(previous);
    return {};
}
bool NickManager::release(SessionId id) noexcept {
    const auto current = owners_.find(id.value());
    if (current == owners_.end()) { return false; }
    names_.erase(current->second);
    owners_.erase(current);
    return true;
}
} // namespace dc24h::users
