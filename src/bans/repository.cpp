#include "dc24h/bans/repository.hpp"
#include "dc24h/accounts/registration.hpp"
#include "dc24h/security/roles.hpp"
#include "registration_internal.hpp"
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <type_traits>

namespace dc24h::bans {
namespace {
constexpr std::string_view columns="id,kind,subject,reason,created_at,expires_at,created_by,revoked_at,revoked_by,revision";
[[noreturn]] void fail(ErrorKind kind){throw Error(kind);}
template<class... T> database::Parameters params(T&&... values){database::Parameters p;p.reserve(sizeof...(T));(p.emplace_back(std::forward<T>(values)),...);return p;}
std::uint64_t number(const database::Cell& cell,bool zero=false){
    std::uint64_t n{};if(!cell){fail(ErrorKind::invalid_record);}const auto [end,error]=std::from_chars(cell->data(),cell->data()+cell->size(),n);
    if(error!=std::errc{}||end!=cell->data()+cell->size()||std::to_string(n)!=*cell||(!zero&&!n)||n>std::uint64_t(std::numeric_limits<std::int64_t>::max())){fail(ErrorKind::invalid_record);}return n;
}
Record decode(const database::Row& row){
    if(row.size()!=10||!row[1]||!row[2]||!row[3]){fail(ErrorKind::invalid_record);}const auto kind=parse_kind(*row[1]);if(!kind){fail(ErrorKind::invalid_record);}
    Record r{number(row[0]),*kind,*row[2],*row[3],static_cast<Timestamp>(number(row[4],true)),{},number(row[6]),{},{},number(row[9])};
    if(row[5]){r.expires_at=static_cast<Timestamp>(number(row[5]));}if(row[7]){r.revoked_at=static_cast<Timestamp>(number(row[7],true));}if(row[8]){r.revoked_by=number(row[8]);}validate(r);return r;
}
struct Erase {database::Result& result;~Erase(){for(auto& row:result.rows){for(auto& cell:row){if(cell){OPENSSL_cleanse(cell->data(),cell->size());}}}}};
std::string lock_name(database::Connection& c){
    const auto db=c.scalar("SELECT DATABASE()");if(!db){fail(ErrorKind::schema);}unsigned char hash[SHA256_DIGEST_LENGTH];SHA256(reinterpret_cast<const unsigned char*>(db->data()),db->size(),hash);
    constexpr char hex[]="0123456789abcdef";std::string name="dc24h:bans:";for(unsigned i=0;i<24;++i){name+=hex[hash[i]>>4];name+=hex[hash[i]&15];}return name;
}
struct Unlock {database::Connection& c;const std::string& name;~Unlock(){try{(void)c.scalar("SELECT RELEASE_LOCK(?)",{name});}catch(...){}}};
bool lower(accounts::Role actor,accounts::Role target){const auto a=security::role_rank(actor),t=security::role_rank(target);return a&&t&&*a>*t;}
}
void Repository::require_schema(){
    const auto status=database::inspect_migrations(connection_);if(status.dirty||status.pending||status.current_version<8){fail(ErrorKind::schema);}
    if(connection_.scalar("SELECT ENGINE FROM information_schema.tables WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_bans'")!="InnoDB"){fail(ErrorKind::schema);}
    const auto result=connection_.query("SELECT COLUMN_NAME,DATA_TYPE,IS_NULLABLE,CHARACTER_MAXIMUM_LENGTH,EXTRA,COLUMN_TYPE FROM information_schema.columns WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_bans' ORDER BY ORDINAL_POSITION",{},{10,6,128,4096});
    constexpr std::array<std::string_view,10> names{"id","kind","subject","reason","created_at","expires_at","created_by","revoked_at","revoked_by","revision"};
    if(result.rows.size()!=names.size()){fail(ErrorKind::schema);}
    for(std::size_t i=0;i<names.size();++i){const auto& row=result.rows[i];const bool bytes=i>=1&&i<=3,nullable=i==5||i==7||i==8;
        if(row[0]!=names[i]||row[1]!=(bytes?"varbinary":"bigint")||row[2]!=(nullable?"YES":"NO")||row[4]!=(i==0?"auto_increment":"")||
            (bytes&&row[3]!=(i==1?"16":i==2?"64":"256"))||(!bytes&&(!row[5]||row[5]->find("unsigned")!=std::string::npos))){fail(ErrorKind::schema);}}
    if(connection_.scalar("SELECT COUNT(*) FROM information_schema.key_column_usage WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_bans' AND REFERENCED_TABLE_SCHEMA=DATABASE() AND REFERENCED_TABLE_NAME='dc24h_accounts' AND REFERENCED_COLUMN_NAME='id' AND ((CONSTRAINT_NAME='dc24h_bans_creator' AND COLUMN_NAME='created_by') OR (CONSTRAINT_NAME='dc24h_bans_revoker' AND COLUMN_NAME='revoked_by'))")!="2"){fail(ErrorKind::schema);}
    if(connection_.scalar("SELECT COUNT(*) FROM information_schema.statistics WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='dc24h_bans' AND INDEX_NAME='PRIMARY' AND COLUMN_NAME='id' AND NON_UNIQUE=0 AND SEQ_IN_INDEX=1 AND SUB_PART IS NULL")!="1"){fail(ErrorKind::schema);}
}
std::optional<Record> Repository::find(std::uint64_t id){
    if(!id||id>std::uint64_t(std::numeric_limits<std::int64_t>::max())){fail(ErrorKind::invalid_input);}require_schema();
    const auto result=connection_.query("SELECT "+std::string(columns)+" FROM dc24h_bans WHERE id=?",params(id),{1,10,256,2048});
    return result.rows.empty()?std::nullopt:std::optional<Record>{decode(result.rows.at(0))};
}
std::vector<Record> Repository::list(std::uint64_t after){
    if(after>std::uint64_t(std::numeric_limits<std::int64_t>::max())){fail(ErrorKind::invalid_input);}require_schema();
    const auto result=connection_.query("SELECT "+std::string(columns)+" FROM dc24h_bans WHERE id>? ORDER BY id LIMIT 16",params(after),{16,10,256,16384});
    std::vector<Record> records;records.reserve(result.rows.size());for(const auto& row:result.rows){records.push_back(decode(row));}return records;
}
std::shared_ptr<const Directory> Repository::load(Timestamp now){
    if(now<0||now>accounts::max_timestamp){fail(ErrorKind::invalid_input);}require_schema();
    auto transaction=connection_.transaction();std::vector<Rule> rules;std::uint64_t after{};
    for(;;){
        const auto result=connection_.query("SELECT "+std::string(columns)+" FROM dc24h_bans WHERE id>? AND revoked_at IS NULL AND (expires_at IS NULL OR expires_at>?) ORDER BY id LIMIT 1024",params(after,now),{1024,10,256,1024*1024});
        for(const auto& row:result.rows){const auto r=decode(row);if(rules.size()==max_active_bans){fail(ErrorKind::capacity);}rules.push_back({r.kind,r.subject,r.expires_at});after=r.id;}
        if(result.rows.size()<1024){break;}
    }
    auto directory=std::make_shared<const Directory>(rules);transaction.commit();return directory;
}
Record Service::create(const accounts::StoredAccount& actor,std::string_view proof,Kind kind,std::string_view subject,std::string_view reason,std::optional<Timestamp> expires,Timestamp now){
    const auto canonical=canonical_subject(kind,subject);if(!canonical){fail(ErrorKind::invalid_input);}
    Record candidate{1,kind,*canonical,std::string(reason),now,expires,actor.account.id().value(),{},{},1};
    try{validate(candidate);}catch(const Error&){fail(ErrorKind::invalid_input);}return perform(actor,proof,std::move(candidate),false,now);
}
Record Service::revoke(const accounts::StoredAccount& actor,std::string_view proof,const Record& expected,Timestamp now){validate(expected);return perform(actor,proof,expected,true,now);}
Record Service::perform(const accounts::StoredAccount& actor,std::string_view proof,Record candidate,bool revoke,Timestamp now){
    using namespace accounts;
    if(!actor.account.valid()||!actor.revision||actor.revision>=std::uint64_t(std::numeric_limits<std::int64_t>::max())||now<actor.account.times().updated_at||now<0||now>max_timestamp){fail(ErrorKind::invalid_input);}
    if(actor.account.status()!=Status::active||!security::allows(actor.account.role(),security::Permission::ban_users)||!security::allows(actor.account.role(),security::Permission::manage_blocks)){fail(ErrorKind::forbidden);}
    if(revoke&&(candidate.revoked_at||now<candidate.created_at||candidate.revision>=std::uint64_t(std::numeric_limits<std::int64_t>::max()))){fail(ErrorKind::stale);}
    detail::validate_password(proof);Repository(connection_).require_schema();AccountRepository accounts(connection_);accounts.require_schema();RegistrationService(connection_).require_credentials_schema();
    if(accounts.find_by_id(actor.account.id())!=actor){fail(ErrorKind::stale);}
    std::optional<StoredAccount> target,creator;
    const auto mapped=[&]()->std::optional<std::uint64_t>{
        if(candidate.kind==Kind::account){return account_subject(candidate.subject);}if(candidate.kind!=Kind::cid){return {};}
        const auto result=connection_.query("SELECT account_id FROM dc24h_account_identities WHERE cid=?",params(candidate.subject),{1,1,32,64});
        return result.rows.empty()?std::nullopt:std::optional<std::uint64_t>{number(result.rows[0][0])};
    };
    const auto target_id=mapped();if(target_id){target=accounts.find_by_id(*AccountId::from_value(*target_id));if(!target){fail(ErrorKind::not_found);}
        if(target->account.id()==actor.account.id()||!lower(actor.account.role(),target->account.role())){fail(ErrorKind::forbidden);}}
    if(revoke&&candidate.created_by!=actor.account.id().value()){
        creator=accounts.find_by_id(*AccountId::from_value(candidate.created_by));if(!creator){fail(ErrorKind::invalid_record);}if(!lower(actor.account.role(),creator->account.role())){fail(ErrorKind::forbidden);}}
    auto captured=connection_.query("SELECT verifier,created_at FROM dc24h_account_credentials WHERE account_id=?",params(actor.account.id().value()),{1,2,192,256});Erase erase{captured};
    if(captured.rows.size()!=1||captured.rows[0].size()!=2||!captured.rows[0][0]||!captured.rows[0][1]||number(captured.rows[0][1],true)<std::uint64_t(actor.account.times().created_at)||number(captured.rows[0][1],true)>std::uint64_t(actor.account.times().updated_at)){fail(ErrorKind::invalid_record);}
    if(!detail::verify_password(proof,*captured.rows[0][0])){fail(ErrorKind::forbidden);}
    const auto name=lock_name(connection_);if(connection_.scalar("SELECT IS_USED_LOCK(?)=CONNECTION_ID()",{name})=="1"){fail(ErrorKind::stale);}Unlock unlock{connection_,name};if(connection_.scalar("SELECT GET_LOCK(?,0)",{name})!="1"){fail(ErrorKind::stale);}
    auto transaction=connection_.transaction();
    std::array<const StoredAccount*,3> locks{&actor,target?&*target:nullptr,creator?&*creator:nullptr};
    std::sort(locks.begin(),locks.end(),[](const auto* a,const auto* b){return a&&(!b||a->account.id().value()<b->account.id().value());});
    std::uint64_t last{};for(const auto* expected:locks){if(expected&&expected->account.id().value()!=last){last=expected->account.id().value();if(accounts.locked_in_transaction(expected->account.id())!=*expected){fail(ErrorKind::stale);}}}
    if(candidate.kind==Kind::cid){const auto current=connection_.query("SELECT account_id FROM dc24h_account_identities WHERE cid=? FOR UPDATE",params(candidate.subject),{1,1,32,64});
        const auto id=current.rows.empty()?std::nullopt:std::optional<std::uint64_t>{number(current.rows[0][0])};if(id!=target_id){fail(ErrorKind::stale);}}
    auto current=connection_.query("SELECT verifier,created_at FROM dc24h_account_credentials WHERE account_id=? FOR UPDATE",params(actor.account.id().value()),{1,2,192,256});Erase erase_current{current};if(current.rows!=captured.rows){fail(ErrorKind::stale);}
    if(revoke){const auto row=connection_.query("SELECT "+std::string(columns)+" FROM dc24h_bans WHERE id=? FOR UPDATE",params(candidate.id),{1,10,256,2048});
        if(row.rows.size()!=1||decode(row.rows[0])!=candidate){fail(ErrorKind::stale);}
        if(connection_.execute_prepared("UPDATE dc24h_bans SET revoked_at=?,revoked_by=?,revision=revision+1 WHERE id=? AND revision=?",params(now,actor.account.id().value(),candidate.id,candidate.revision)).affected_rows!=1){fail(ErrorKind::stale);}
        candidate.revoked_at=now;candidate.revoked_by=actor.account.id().value();++candidate.revision;
    }else{
        if(number(connection_.scalar("SELECT COUNT(*) FROM dc24h_bans WHERE revoked_at IS NULL AND (expires_at IS NULL OR expires_at>?)",{std::to_string(now)}),true)>=max_active_bans){fail(ErrorKind::capacity);}
        const auto written=connection_.execute_prepared("INSERT INTO dc24h_bans(kind,subject,reason,created_at,expires_at,created_by) VALUES (?,?,?,?,?,?)",params(std::string(kind_key(candidate.kind)),candidate.subject,candidate.reason,now,candidate.expires_at?database::Parameter{*candidate.expires_at}:database::Parameter{nullptr},actor.account.id().value()));
        if(written.affected_rows!=1||!written.insert_id){fail(ErrorKind::invalid_record);}candidate.id=written.insert_id;
    }
    static_assert(std::is_nothrow_move_constructible_v<Record>);transaction.commit();return candidate;
}
}
