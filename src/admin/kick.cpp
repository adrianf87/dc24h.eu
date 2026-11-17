#include "dc24h/admin/kick.hpp"
namespace dc24h::admin {
KickError KickCore::check(users::SessionId actor,users::SessionId target,users::TimePoint now) const noexcept {
    const auto source=sessions_.authenticated(actor);
    const auto* destination=sessions_.find(target);
    if(!source){return KickError::forbidden;}
    if(actor==target){return KickError::forbidden;}
    if(!destination||!destination->ready()){return KickError::unavailable;}
    if(now<source->established_at){return KickError::invalid_time;}
    const auto auth=destination->authentication();
    if(!security::can_revoke(source->role,auth?auth->role:accounts::Role::guest,policy_)){return KickError::forbidden;}
    return KickError::none;
}
KickError KickCore::apply(users::SessionId actor,users::SessionId target,users::TimePoint now) noexcept {
    const auto error=check(actor,target,now);if(error!=KickError::none){return error;}
    if(sessions_.begin_close(target,users::CloseCause::kicked,now)){return KickError::invalid_time;}
    return KickError::none;
}
}
