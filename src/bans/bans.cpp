#include "dc24h/bans/bans.hpp"
#include "dc24h/adc/tiger_identity.hpp"
#include "dc24h/text/public_text.hpp"
#include <arpa/inet.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <limits>

namespace dc24h::bans {
namespace {
constexpr auto maximum=std::uint64_t(std::numeric_limits<std::int64_t>::max());
[[noreturn]] void fail(ErrorKind kind){throw Error(kind);}
std::optional<Decision> active(std::optional<Timestamp> expiry,Timestamp now) noexcept {
    return !expiry||*expiry>now?std::optional<Decision>{{expiry}}:std::nullopt;
}
template<class Map,class Key> void merge(Map& map,Key&& key,std::optional<Timestamp> expiry){
    const auto [entry,inserted]=map.emplace(std::forward<Key>(key),expiry);
    if(!inserted&&entry->second){if(!expiry){entry->second.reset();}else{entry->second=std::max(*entry->second,*expiry);}}
}
}
const char* Error::what() const noexcept {
    switch(kind_){
    case ErrorKind::invalid_input:return "Invalid ban input";case ErrorKind::forbidden:return "Ban authorization refused";
    case ErrorKind::stale:return "Ban revision is stale";case ErrorKind::not_found:return "Ban target not found";
    case ErrorKind::schema:return "Bans require current compatible storage";case ErrorKind::invalid_record:return "Invalid stored ban";
    case ErrorKind::capacity:return "Active ban capacity reached";}
    return "Invalid ban input";
}
std::string_view kind_key(Kind kind) noexcept {switch(kind){case Kind::account:return "account";case Kind::cid:return "cid";case Kind::ip:return "ip";}return {};}
std::optional<Kind> parse_kind(std::string_view key) noexcept {for(auto kind:{Kind::account,Kind::cid,Kind::ip}){if(kind_key(kind)==key){return kind;}}return {};}
std::optional<std::uint64_t> account_subject(std::string_view text) noexcept {
    std::uint64_t value{};const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty()||text.front()=='0'||error!=std::errc{}||end!=text.data()+text.size()||!value||value>maximum){return {};}
    return value;
}
std::optional<std::string> canonical_subject(Kind kind,std::string_view text){
    if(kind==Kind::account){if(!account_subject(text)){return {};}return std::string(text);}
    if(kind==Kind::cid){if(!adc::TigerCid::parse(text)){return {};}return std::string(text);}
    if(kind!=Kind::ip||text.empty()||text.size()>=INET6_ADDRSTRLEN||text.find('\0')!=std::string_view::npos){return {};}
    char input[INET6_ADDRSTRLEN]{},output[INET6_ADDRSTRLEN]{};std::copy(text.begin(),text.end(),input);
    in6_addr v6{};in_addr v4{};int family=AF_INET;const void* bytes=&v4;
    if(inet_pton(AF_INET,input,&v4)!=1){if(inet_pton(AF_INET6,input,&v6)!=1){return {};}family=AF_INET6;bytes=&v6;
        if(IN6_IS_ADDR_V4MAPPED(&v6)){family=AF_INET;bytes=&v6.s6_addr[12];}}
    if(!inet_ntop(family,bytes,output,sizeof(output))){return {};}return std::string(output);
}
std::string_view state_key(State state) noexcept {
    switch(state){case State::permanent:return "permanent";case State::temporary:return "temporary";
    case State::expired:return "expired";case State::revoked:return "revoked";}return {};
}
std::string_view filter_key(Filter filter) noexcept {
    switch(filter){case Filter::all:return "all";case Filter::active:return "active";case Filter::permanent:return "permanent";
    case Filter::temporary:return "temporary";case Filter::expired:return "expired";case Filter::revoked:return "revoked";}return {};
}
std::optional<Filter> parse_filter(std::string_view key) noexcept {
    for(auto filter:{Filter::all,Filter::active,Filter::permanent,Filter::temporary,Filter::expired,Filter::revoked}){
        if(filter_key(filter)==key){return filter;}
    }return {};
}
State state_at(const Record& r,Timestamp now) noexcept {
    if(r.revoked_at){return State::revoked;}if(!r.expires_at){return State::permanent;}
    return *r.expires_at>now?State::temporary:State::expired;
}
std::optional<Timestamp> seconds_left(const Record& r,Timestamp now) noexcept {
    if(state_at(r,now)!=State::temporary){return {};}
    // Valid UTC inputs keep subtraction within the Timestamp range.
    return *r.expires_at-std::clamp(now,Timestamp{0},accounts::max_timestamp);
}
Timestamp expires_after(Timestamp now,std::uint64_t seconds){
    if(now<0||now>accounts::max_timestamp||!seconds||seconds>max_duration_seconds||
        seconds>static_cast<std::uint64_t>(accounts::max_timestamp-now)){fail(ErrorKind::invalid_input);}
    return now+static_cast<Timestamp>(seconds);
}
void validate(const Record& r){
    const auto subject=canonical_subject(r.kind,r.subject);
    if(!r.id||r.id>maximum||!subject||*subject!=r.subject||r.reason.empty()||r.reason.size()>256||!text::valid_public_text(r.reason)||
        r.created_at<0||r.created_at>accounts::max_timestamp||!r.created_by||r.created_by>maximum||!r.revision||r.revision>maximum||
        (r.expires_at&&(*r.expires_at<=r.created_at||*r.expires_at>accounts::max_timestamp))||
        bool(r.revoked_at)!=bool(r.revoked_by)||(r.revoked_at&&(*r.revoked_at<r.created_at||*r.revoked_at>accounts::max_timestamp))||
        (r.revoked_by&&(!*r.revoked_by||*r.revoked_by>maximum))){fail(ErrorKind::invalid_record);}
}
Directory::Directory(const std::vector<Rule>& rules){
    if(rules.size()>max_active_bans){fail(ErrorKind::capacity);}
    for(const auto& rule:rules){const auto subject=canonical_subject(rule.kind,rule.subject);
        if(!subject||*subject!=rule.subject||(rule.expires_at&&(*rule.expires_at<=0||*rule.expires_at>accounts::max_timestamp))){fail(ErrorKind::invalid_record);}
        switch(rule.kind){case Kind::account:merge(accounts_,*account_subject(rule.subject),rule.expires_at);break;
        case Kind::cid:merge(cids_,rule.subject,rule.expires_at);break;
        case Kind::ip:merge(ips_,*security::AuthenticationAddress::parse(rule.subject),rule.expires_at);break;}}
}
std::optional<Decision> Directory::ip(std::string_view value,Timestamp time) const noexcept {const auto key=security::AuthenticationAddress::parse(value);if(!key){return {};}
    const auto entry=ips_.find(*key);return entry==ips_.end()?std::nullopt:active(entry->second,time);}
std::optional<Decision> Directory::cid(std::string_view value,Timestamp time) const noexcept {const auto entry=cids_.find(value);return entry==cids_.end()?std::nullopt:active(entry->second,time);}
std::optional<Decision> Directory::account(std::uint64_t value,Timestamp time) const noexcept {const auto entry=accounts_.find(value);return entry==accounts_.end()?std::nullopt:active(entry->second,time);}
Timestamp Directory::now() noexcept{return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
}
