#include "dc24h/admin/core.hpp"
#include "dc24h/accounts/password_security.hpp"
#include "dc24h/accounts/registration.hpp"
#include "dc24h/security/roles.hpp"
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace dc24h::admin {
namespace {
[[noreturn]] void fail(ErrorKind kind) { throw Error(kind); }
constexpr auto maximum = std::uint64_t(std::numeric_limits<std::int64_t>::max());
bool valid_id(std::uint64_t id) noexcept { return id && id <= maximum; }
bool empty(AccountRef ref) noexcept { return !ref.id && !ref.revision; }
void increment(std::uint64_t& value) noexcept { if (value != std::numeric_limits<std::uint64_t>::max()) { ++value; } }
struct Record {
    Statistics& statistics;
    ErrorKind outcome{ErrorKind::unavailable};
    bool success{};
    ~Record() {
        if (success) { increment(statistics.succeeded); }
        else if (outcome == ErrorKind::outcome_unknown) { increment(statistics.outcome_unknown); }
        else if (outcome == ErrorKind::unavailable) { increment(statistics.unavailable); }
        else { increment(statistics.refused); }
    }
};
accounts::StoredAccount find(accounts::AccountRepository& repository, AccountRef ref) {
    const auto result = repository.find_by_id(*accounts::AccountId::from_value(ref.id));
    if (!result) { fail(ErrorKind::not_found); }
    if (ref.revision && ref.revision != result->revision) { fail(ErrorKind::stale); }
    return *result;
}
void validate(const Command& c) {
    if (action_key(c.action).empty() || c.time < 0 || c.time > accounts::max_timestamp ||
        c.proof.size()>1024 || c.password.size()>1024 || c.successor_proof.size()>1024 || c.name.size()>64) { fail(ErrorKind::invalid_input); }
    const bool ban=c.action==Action::ban_create||c.action==Action::ban_revoke;
    if(ban){
        if(!valid_id(c.actor.id)||c.actor.revision>maximum||c.proof.empty()||!c.password.empty()||!c.name.empty()||!c.successor_proof.empty()||c.role!=accounts::Role::registered){fail(ErrorKind::invalid_input);}
        if(c.action==Action::ban_create){
            if(!empty(c.target)||c.subject.empty()||c.reason.empty()||!bans::parse_kind(bans::kind_key(c.ban_kind))||(c.expires_at&&c.duration_seconds)){fail(ErrorKind::invalid_input);}
            if(c.duration_seconds){(void)bans::expires_after(c.time,*c.duration_seconds);}
        }
        else if(!valid_id(c.target.id)||!valid_id(c.target.revision)||!c.subject.empty()||!c.reason.empty()||c.expires_at||c.duration_seconds||c.ban_kind!=bans::Kind::account){fail(ErrorKind::invalid_input);}
        return;
    }
    if(!c.subject.empty()||!c.reason.empty()||c.expires_at||c.duration_seconds||c.ban_kind!=bans::Kind::account){fail(ErrorKind::invalid_input);}
    const bool creation = c.action == Action::create || c.action == Action::bootstrap_owner;
    const bool bootstrap = c.action == Action::bootstrap_owner;
    const bool transfer = c.action == Action::transfer_owner;
    const bool own = c.action == Action::password;
    if (creation ? !empty(c.target) : (!valid_id(c.target.id) || !valid_id(c.target.revision))) { fail(ErrorKind::invalid_input); }
    if (bootstrap || own) { if (!empty(c.actor)) { fail(ErrorKind::invalid_input); } }
    else if (!valid_id(c.actor.id) || c.actor.revision > maximum || (transfer && !valid_id(c.actor.revision))) { fail(ErrorKind::invalid_input); }
    if (bootstrap ? !c.proof.empty() : c.proof.empty()) { fail(ErrorKind::invalid_input); }
    const bool password = creation || own || c.action == Action::reset;
    if (password ? c.password.empty() : !c.password.empty()) { fail(ErrorKind::invalid_input); }
    if (creation ? c.name.empty() : !c.name.empty()) { fail(ErrorKind::invalid_input); }
    if (transfer ? c.successor_proof.empty() : !c.successor_proof.empty()) { fail(ErrorKind::invalid_input); }
    if (c.action != Action::role && c.role != accounts::Role::registered) { fail(ErrorKind::invalid_input); }
    if (c.action == Action::role && !accounts::account_role(c.role)) { fail(ErrorKind::forbidden); }
}
}
std::string_view action_key(Action action) noexcept {
    switch (action) {
    case Action::create: return "accounts.create"; case Action::activate: return "accounts.activate";
    case Action::disable: return "accounts.disable"; case Action::password: return "accounts.password";
    case Action::reset: return "accounts.reset"; case Action::role: return "accounts.role";
    case Action::ban_create: return "bans.create";case Action::ban_revoke:return "bans.revoke";
    case Action::bootstrap_owner: return "ownership.bootstrap"; case Action::transfer_owner: return "ownership.transfer";
    }
    return {};
}
std::optional<Action> parse_action(std::string_view key) noexcept {
    for (auto action : action_catalog) { if (action_key(action) == key) { return action; } }
    return std::nullopt;
}
bool available(Action action, Origin origin) noexcept {
    if (action_key(action).empty()) { return false; }
    const bool owner = action == Action::bootstrap_owner || action == Action::transfer_owner;
    return (origin == Origin::owner_cli && owner) || (origin == Origin::maintenance_api && !owner);
}
const char* Error::what() const noexcept {
    switch (kind_) {
    case ErrorKind::invalid_input: return "Invalid administrative command";
    case ErrorKind::forbidden: return "Administrative command refused";
    case ErrorKind::stale: return "Administrative account revision is stale";
    case ErrorKind::not_found: return "Administrative account not found";
    case ErrorKind::unavailable: return "Administrative service unavailable";
    case ErrorKind::outcome_unknown: return "Administrative write outcome unknown";
    }
    return "Administrative command refused";
}
Engine::Engine(database::Options options, Origin origin) : options_(std::move(options)), origin_(origin) {
    database::validate(options_);
    if (origin != Origin::maintenance_api && origin != Origin::owner_cli) { fail(ErrorKind::invalid_input); }
}
Result Engine::execute(const Command& command) {
    increment(statistics_.attempts); Record record{statistics_};
    try {
        try {
            if (!available(command.action,origin_)) { fail(ErrorKind::forbidden); }
            validate(command);
            database::Connection connection(options_); accounts::AccountRepository repository(connection);
            accounts::AccountManagementService service(connection);
            Result result = [&]() -> Result {
                if(command.action==Action::ban_create||command.action==Action::ban_revoke){
                    const auto actor=find(repository,command.actor);bans::Service bans(connection);
                    if(command.action==Action::ban_create){return bans.create(actor,command.proof,command.ban_kind,command.subject,command.reason,command.duration_seconds?std::optional<bans::Timestamp>{bans::expires_after(command.time,*command.duration_seconds)}:command.expires_at,command.time);}
                    const auto expected=dc24h::bans::Repository(connection).find(command.target.id);
                    if(!expected){fail(ErrorKind::not_found);}if(expected->revision!=command.target.revision){fail(ErrorKind::stale);}
                    return bans.revoke(actor,command.proof,*expected,command.time);
                }
                if (command.action == Action::bootstrap_owner) { return service.bootstrap_owner(command.name,command.password,command.time); }
                if (command.action == Action::create) {
                    const auto actor = find(repository,command.actor);
                    return service.create_account(actor,command.proof,command.name,command.password,command.time);
                }
                const auto target = find(repository,command.target);
                if (command.action == Action::password) { return service.change_password(target,command.proof,command.password,command.time); }
                const auto actor = find(repository,command.actor);
                switch (command.action) {
                case Action::activate: return service.activate_account(actor,command.proof,target,command.time);
                case Action::disable: return service.disable_account(actor,command.proof,target,command.time);
                case Action::reset: return service.reset_password(actor,command.proof,target,command.password,command.time);
                case Action::role: return service.change_role(actor,command.proof,target,command.role,command.time);
                case Action::transfer_owner: return service.transfer_ownership(actor,command.proof,target,command.successor_proof,command.time);
                default: fail(ErrorKind::invalid_input);
                }
            }();
            static_assert(std::is_nothrow_move_constructible_v<Result>);
            record.success = true; return result;
        } catch(const bans::Error& e){
            fail(e.kind()==bans::ErrorKind::forbidden?ErrorKind::forbidden:e.kind()==bans::ErrorKind::stale?ErrorKind::stale:e.kind()==bans::ErrorKind::not_found?ErrorKind::not_found:e.kind()==bans::ErrorKind::invalid_input?ErrorKind::invalid_input:ErrorKind::unavailable);
        } catch (const accounts::ManagementError& e) {
            fail(e.kind()==accounts::ManagementErrorKind::stale ? ErrorKind::stale :
                 e.kind()==accounts::ManagementErrorKind::unauthorized ? ErrorKind::forbidden :
                 e.kind()==accounts::ManagementErrorKind::schema || e.kind()==accounts::ManagementErrorKind::crypto || e.kind()==accounts::ManagementErrorKind::invalid_record ? ErrorKind::unavailable : ErrorKind::invalid_input);
        } catch (const accounts::RegistrationError& e) {
            fail(e.kind()==accounts::RegistrationErrorKind::crypto || e.kind()==accounts::RegistrationErrorKind::schema ? ErrorKind::unavailable : ErrorKind::invalid_input);
        } catch (const accounts::RepositoryError& e) {
            fail(e.kind()==accounts::RepositoryErrorKind::duplicate || e.kind()==accounts::RepositoryErrorKind::stale ? ErrorKind::stale : ErrorKind::unavailable);
        } catch (const accounts::PasswordSecurityError&) { fail(ErrorKind::unavailable);
        } catch (const database::Error& e) { fail(e.outcome_unknown() ? ErrorKind::outcome_unknown : ErrorKind::unavailable); }
        catch (const Error&) { throw; }
        catch (const std::bad_alloc&) { throw; }
        catch (...) { fail(ErrorKind::unavailable); }
    } catch (const Error& e) { record.outcome = e.kind(); throw; }
}
accounts::StoredAccount Engine::account(std::uint64_t id) {
    if (!valid_id(id)) { fail(ErrorKind::invalid_input); }
    database::Connection connection(options_); accounts::AccountRepository repository(connection);
    return find(repository,{id,0});
}
std::vector<accounts::StoredAccount> Engine::list(std::uint64_t after, bool staff) {
    if (after > maximum) { fail(ErrorKind::invalid_input); }
    database::Connection connection(options_); accounts::AccountRepository repository(connection);
    return staff ? repository.list_staff_after(after) : repository.list_after(after);
}
std::vector<bans::Record> Engine::bans_list(std::uint64_t after,bans::Filter filter,bans::Timestamp now){
    database::Connection connection(options_);return bans::Repository(connection).list(after,filter,now);
}
std::vector<audit::Event> Engine::audit_list(std::uint64_t actor, std::string_view proof, std::uint64_t after) {
    if (!valid_id(actor) || after > maximum || proof.size() > 1024) { fail(ErrorKind::invalid_input); }
    try {
        database::Connection connection(options_);
        accounts::AccountRepository repository(connection);
        const auto stored = repository.find_by_id(*accounts::AccountId::from_value(actor));
        if (!stored) { fail(ErrorKind::not_found); }
        if (stored->account.status() != accounts::Status::active || !security::allows(stored->account.role(), security::Permission::read_audit)) {
            fail(ErrorKind::forbidden);
        }
        const auto verdict = accounts::PasswordSecurityService(connection).verify(stored->account.id(), proof);
        if (verdict == accounts::PasswordVerification::unavailable) { fail(ErrorKind::unavailable); }
        if (verdict != accounts::PasswordVerification::matched) { fail(ErrorKind::forbidden); }
        return audit::Repository(connection).list(after);
    } catch (const audit::Error&) { fail(ErrorKind::unavailable);
    } catch (const accounts::PasswordSecurityError& error) {
        fail(error.kind() == accounts::PasswordSecurityErrorKind::invalid_password ? ErrorKind::invalid_input : ErrorKind::unavailable);
    } catch (const accounts::RepositoryError&) { fail(ErrorKind::unavailable);
    } catch (const database::Error& error) { fail(error.outcome_unknown() ? ErrorKind::outcome_unknown : ErrorKind::unavailable);
    } catch (const Error&) { throw; }
    catch (const std::bad_alloc&) { throw; }
    catch (...) { fail(ErrorKind::unavailable); }
}
} // namespace dc24h::admin
