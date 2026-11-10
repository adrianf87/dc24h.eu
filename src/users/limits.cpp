#include "dc24h/users/limits.hpp"

#include <algorithm>
#include <stdexcept>

namespace dc24h::users {
void validate(const Limits& limits) {
    if (limits.max_sessions==0 || limits.max_sessions>Limits::max_capacity ||
        limits.max_users==0 || limits.max_users>Limits::max_capacity) {
        throw std::invalid_argument("Invalid user/session limits");
    }
}
Limits Limits::bounded_by(std::size_t max_connections) const {
    validate(*this);
    if (max_connections==0 || max_connections>max_capacity) {
        throw std::invalid_argument("Invalid user/session limits");
    }
    const auto sessions=std::min(max_sessions,max_connections);
    return {sessions,std::min(max_users,sessions)};
}
} // namespace dc24h::users
