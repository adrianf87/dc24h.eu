#include "dc24h/admin/mute.hpp"
#include "dc24h/security/moderation.hpp"
namespace dc24h::admin {
std::string_view mute_scope_key(MuteScope scope) noexcept {
    switch(scope){case MuteScope::public_chat:return "public";case MuteScope::private_chat:return "private";case MuteScope::all:return "all";}return {};
}
std::optional<MuteScope> parse_mute_scope(std::string_view key) noexcept {
    for(auto scope:{MuteScope::public_chat,MuteScope::private_chat,MuteScope::all}){if(mute_scope_key(scope)==key){return scope;}}return {};
}
MuteError MuteCore::check(users::SessionId actor,users::SessionId target,MuteScope scope,users::TimePoint now) const noexcept {
    if(mute_scope_key(scope).empty()){return MuteError::invalid_scope;}
    const auto source=sessions_.authenticated(actor);
    if(!source||actor==target||!security::allows(source->role,security::Permission::mute_users,policy_)){return MuteError::forbidden;}
    const auto* destination=sessions_.find(target);
    if(!destination||!destination->ready()){return MuteError::unavailable;}
    if(now<source->established_at){return MuteError::invalid_time;}
    const auto auth=destination->authentication();
    // Unmute uses the same permission and target rule as mute.
    security::ModerationTarget view;
    view.subject=security::ModerationSubject::session;
    view.role=auth?auth->role:accounts::Role::guest;
    return security::moderate(security::ModerationOperation::mute,{source->role},view,policy_).refusal==security::ModerationRefusal::none?MuteError::none:MuteError::forbidden;
}
MuteError MuteCore::apply(users::SessionId actor,users::SessionId target,MuteScope scope,bool enabled,users::TimePoint now) noexcept {
    const auto error=check(actor,target,scope,now);if(error!=MuteError::none){return error;}
    auto state=sessions_.find(target)->chat_mute();
    if(scope!=MuteScope::private_chat){state.public_chat=enabled;}
    if(scope!=MuteScope::public_chat){state.private_chat=enabled;}
    return sessions_.set_chat_mute(target,state,now)?MuteError::invalid_time:MuteError::none;
}
}
