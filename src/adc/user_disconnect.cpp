#include "dc24h/adc/user_disconnect.hpp"

namespace dc24h::adc {
DepartureResult UserDisconnect::begin(users::SessionId id, users::CloseCause cause, users::TimePoint now) noexcept {
    const auto* model = sessions_.find(id);
    if (!model) { return users::RegistryError::missing; }
    Departure departure{id,{},model->phase()!=users::Phase::closed,model->was_ready(),false};
    if (departure.first && model->identity()) {
        departure.public_sid = Sid::parse(model->identity()->sid());
        if (!departure.public_sid || departure.public_sid->value()==0) {
            departure.public_sid.reset(); departure.invalid_public_sid = true;
        }
    }
    if (const auto error = sessions_.finish_close(id,cause,now)) {
        return std::visit([](auto value) -> DepartureResult { return value; },*error);
    }
    return departure;
}
std::optional<users::RegistryError> UserDisconnect::release(users::SessionId id) noexcept {
    return sessions_.remove(id);
}
} // namespace dc24h::adc
