#pragma once

#include "dc24h/adc/inf.hpp"
#include "dc24h/users/user.hpp"

namespace dc24h::adc {
// Project a validated ClientInfo into the public Users model. CID comes from
// its VerifiedIdentity, never PID. Nonzero SID is supplied by the adapter;
// this does not prove SID lease ownership or registry uniqueness/readiness.
[[nodiscard]] std::optional<users::User> user_from_info(const ClientInfo& info, Sid sid);
} // namespace dc24h::adc
