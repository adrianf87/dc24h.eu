#include "dc24h/security/roles.hpp"
#include "dc24h/security/moderation.hpp"
#include <stdexcept>

namespace dc24h::security {
namespace {
constexpr std::uint32_t bit(Permission p) noexcept {
    const auto n=static_cast<unsigned>(p);return n<32?(std::uint32_t{1}<<n):0;
}
constexpr auto vip_mask=bit(Permission::revoke_sessions)|bit(Permission::mute_users)|bit(Permission::read_audit);
}
std::string_view permission_key(Permission p) noexcept {
    switch(p) {
    case Permission::hub_use:return "hub.use";
    case Permission::public_chat:return "chat.public";case Permission::private_chat:return "chat.private";
    case Permission::search_queries:return "search.queries";case Permission::search_results:return "search.results";
    case Permission::initiate_transfers:return "transfers.initiate";case Permission::receive_transfers:return "transfers.receive";
    case Permission::revoke_sessions:return "users.kick";case Permission::mute_users:return "users.mute";
    case Permission::ban_users:return "users.ban";case Permission::manage_blocks:return "blocks.manage";
    case Permission::manage_accounts:return "accounts.manage";case Permission::manage_roles:return "roles.manage";
    case Permission::read_audit:return "audit.read";case Permission::manage_configuration:return "configuration.manage";
    case Permission::manage_hub:return "hub.manage";
    case Permission::change_own_password:return "accounts.password.change";
    }
    return {};
}
std::optional<Permission> parse_permission(std::string_view key) noexcept {
    for(auto p:permission_catalog){if(permission_key(p)==key)return p;}return std::nullopt;
}
std::optional<unsigned> role_rank(accounts::Role role) noexcept {
    for(unsigned i=0;i<accounts::role_catalog.size();++i){if(accounts::role_catalog[i]==role)return i;}return std::nullopt;
}
bool permission_implemented(Permission p) noexcept {
    if(permission_key(p).empty())return false;
    return p!=Permission::manage_configuration&&p!=Permission::manage_hub;
}
void validate(const RolePolicy& policy) {
    if(policy.vip_permissions&~vip_mask)throw std::invalid_argument("Invalid role permission configuration");
}
RolePolicy parse_vip_permissions(std::string_view text) {
    RolePolicy policy;if(text=="none")return policy;
    if(text.empty()||text.size()>128)throw std::invalid_argument("Invalid role permission configuration");
    while(!text.empty()) {
        const auto comma=text.find(',');auto word=text.substr(0,comma);
        while(!word.empty()&&(word.front()==' '||word.front()=='\t'))word.remove_prefix(1);
        while(!word.empty()&&(word.back()==' '||word.back()=='\t'))word.remove_suffix(1);
        const auto permission=parse_permission(word);
        if(!permission||!(bit(*permission)&vip_mask)||(policy.vip_permissions&bit(*permission)))throw std::invalid_argument("Invalid role permission configuration");
        policy.vip_permissions|=bit(*permission);
        if(comma==std::string_view::npos){break;}
        text.remove_prefix(comma+1);
        if(text.empty())throw std::invalid_argument("Invalid role permission configuration");
    }
    return policy;
}
bool allows(accounts::Role role,Permission p,RolePolicy policy) noexcept {
    if(policy.vip_permissions&~vip_mask)return false;
    const auto rank=role_rank(role);if(!rank||permission_key(p).empty())return false;
    if(role==accounts::Role::owner)return true;
    switch(p) {
    case Permission::hub_use:case Permission::public_chat:case Permission::private_chat:
    case Permission::search_queries:case Permission::search_results:
    case Permission::initiate_transfers:case Permission::receive_transfers:return true;
    case Permission::revoke_sessions:case Permission::mute_users:case Permission::read_audit:
        return *rank>=3||(role==accounts::Role::vip&&(policy.vip_permissions&vip_mask&bit(p)));
    case Permission::ban_users:case Permission::manage_blocks:return *rank>=4;
    case Permission::manage_accounts:case Permission::manage_roles:case Permission::manage_configuration:return *rank>=5;
    case Permission::manage_hub:return false;
    case Permission::change_own_password:return *rank>=1;
    }
    return false;
}
bool can_mute(accounts::Role actor,accounts::Role target,RolePolicy policy) noexcept {
    ModerationTarget view;
    view.subject=ModerationSubject::session;view.role=target;
    return moderate(ModerationOperation::mute,ModerationActor{actor},view,policy).refusal==ModerationRefusal::none;
}
bool can_revoke(accounts::Role actor,accounts::Role target,RolePolicy policy) noexcept {
    ModerationTarget view;
    view.subject=ModerationSubject::session;view.role=target;
    return moderate(ModerationOperation::kick,ModerationActor{actor},view,policy).refusal==ModerationRefusal::none;
}
bool can_manage(accounts::Role actor,accounts::Role target) noexcept {
    const auto a=role_rank(actor),t=role_rank(target);
    return accounts::account_role(target)&&a&&t&&*a>*t&&allows(actor,Permission::manage_accounts);
}
bool can_assign(accounts::Role actor,accounts::Role target,accounts::Role proposed) noexcept {
    const auto a=role_rank(actor),p=role_rank(proposed);
    return can_manage(actor,target)&&accounts::account_role(proposed)&&a&&p&&*a>*p&&allows(actor,Permission::manage_roles);
}
}
