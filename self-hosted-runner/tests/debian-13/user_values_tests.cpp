#include "dc24h/users/session_manager.hpp"
#include "dc24h/text/public_text.hpp"
#include "dc24h/adc/user.hpp"

#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace dc24h;
using namespace std::chrono_literals;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
void checked_text() {
    static_assert(!std::is_default_constructible_v<users::Nickname>);
    static_assert(!std::is_constructible_v<users::Nickname,std::string_view>);
    static_assert(std::is_nothrow_move_constructible_v<users::Nickname>);
    const std::vector<std::string> invalid{
        "\x80", "\xc0\xaf", "\xe2\x82", "\xed\xa0\x80", "\xf4\x90\x80\x80",
        std::string("A\0B",3), "A\nB", "A\tB", "A\x7f", "A\xc2\x85",
        "A\xe2\x80\x8b", "A\xe2\x80\xae", "A\xe2\x80\xa8", "A\xe2\x80\xa9",
        "\xef\xbb\xbf", "e\xcc\x81"
    };
    for (const auto& value : invalid) {
        require(!text::valid_public_text(value) && !users::Nickname::create(value) &&
            !users::Identity::create("SID","CID",value),"Malformed/non-NFC/control identity accepted");
        for (auto member : {&users::ProfileData::description,&users::ProfileData::email,&users::ProfileData::application,
                &users::ProfileData::version,&users::ProfileData::locale}) {
            users::ProfileData data;data.*member=value;
            require(!users::Profile::create(std::move(data)),"Malformed profile text accepted");
        }
    }
    require(text::valid_public_text("") && users::Profile::create({}) && !users::Nickname::create(""),"Empty text/name semantics");
    require(!users::Nickname::create("Has space") && !users::Nickname::create(" Leading") && !users::Nickname::create("Trailing "),"ASCII nickname space accepted");
    users::ProfileData data;data.description=" Zażółć gęślą jaźń ";
    const auto profile=users::Profile::create(data);
    require(profile && profile->data()==data,"Valid UTF-8 profile changed");
    std::string maximum;
    for (unsigned i=0;i<128;++i) { maximum+="é"; }
    require(maximum.size()==users::max_nickname_bytes && users::Nickname::create(maximum) &&
        !users::Nickname::create(maximum+"!"),"Nickname byte bounds confused with code points");
    require(text::valid_public_text(std::string(4096,'A')) && !text::valid_public_text(std::string(4097,'A')),"Shared text temporary storage bound");
    const auto name=*users::Nickname::create("Żółw");
    auto original=*users::User::create(*users::Identity::create("SID","CID",name),*profile);
    const auto renamed=original.with_nickname(*users::Nickname::create("Nowy"));
    require(renamed && renamed->identity().sid()==original.identity().sid() && renamed->identity().cid()==original.identity().cid() &&
        renamed->profile()==original.profile() && original.identity().nickname()=="Żółw" && name.text()=="Żółw" &&
        original.identity().nickname_value()==name,"Detached rename aliases or rebinds identity/profile");
    users::SessionManager registry(1);
    require(std::holds_alternative<const users::Session*>(registry.add(id(1),users::Transport::adc,{})) &&
        !registry.start_negotiation(id(1),{}) && !registry.identify(id(1),original,users::TimePoint{}+1ms),"Registry setup");
    const auto before=*registry.snapshot(id(1));
    for (const auto& bad : invalid) {
        const auto result=registry.rename(id(1),bad,users::TimePoint{}+2ms);
        require(result && std::get<users::SessionError>(*result)==users::SessionError::invalid_identity &&
            registry.snapshot(id(1))==before && registry.by_nickname("Żółw")->id()==id(1),"Rejected name mutated model/indexes");
    }
    require(!registry.update_user(id(1),*renamed,users::TimePoint{}+2ms) && !registry.by_nickname("Żółw") &&
        registry.by_nickname("Nowy")->user()->profile()==*profile && before.identity->nickname()=="Żółw","Checked value failed managed commit");
    users::Session standalone(id(2),users::Transport::adc,{});
    require(!standalone.start_negotiation({}) && !standalone.identify(original,{}),"Standalone setup");
    const auto saved=standalone.snapshot();
    require(standalone.rename("e\xcc\x81",users::TimePoint{}+1ms)==users::SessionError::invalid_identity &&
        standalone.snapshot()==saved,"Standalone rename bypassed NFC");
}
adc::Message parse(std::string_view line) { return std::get<adc::Message>(adc::parse_line(line)); }
void projection() {
    adc::InfValidator validator("127.0.0.1");
    const auto sid=*adc::Sid::parse("AAAB");
    adc::TigerDigest pid{};
    const auto cid=adc::tiger_base32(*adc::tiger192(pid));
    const auto first=parse("BINF AAAB ID"+cid+" PD"+adc::tiger_base32(pid)+
        " NIŻółw DEZażółć\\sgęślą EMpublic@example.test APKlient VE1.0 LOpl_PL SS0 SF7 SL2 SUTCP4,ADC0 I40.0.0.0 RFsource XXprivate");
    const auto validated=validator.validate(first,sid,adc::SessionHash::tiger192);
    require(std::holds_alternative<adc::ClientInfo>(validated),"Projection fixture rejected");
    const auto& info=std::get<adc::ClientInfo>(validated);
    const auto value=adc::user_from_info(info,sid);
    require(value && value->identity().sid()=="AAAB" && value->identity().cid()==info.identity()->cid().text() &&
        value->identity().nickname_value().text()=="Żółw","Projection did not bind verified public CID, SID and checked nick");
    const auto& data=value->profile().data();
    require(data.description=="Zażółć gęślą" && data.email=="public@example.test" && data.application=="Klient" &&
        data.version=="1.0" && data.locale=="pl_PL" && data.shared_bytes==0 && data.shared_files==7 && data.slots==2 &&
        data.features==std::vector<std::string>{"TCP4","ADC0"} && data.ipv4=="127.0.0.1" && data.ipv6.empty() && !data.udp4_port && !data.udp6_port,"Projection changed public fields");
    require(!adc::user_from_info(info,*adc::Sid::from_value(0)),"Reserved zero SID projected as user");
    const auto update=validator.validate(parse("BINF AAAB NINowy DE EM AP VE LO SS SF SL SU"),sid,adc::SessionHash::tiger192,&info);
    require(std::holds_alternative<adc::ClientInfo>(update),"Cancellation rejected");
    const auto newer=adc::user_from_info(std::get<adc::ClientInfo>(update),sid);
    users::ProfileData remaining;remaining.ipv4="127.0.0.1";
    require(newer && newer->identity().cid()==cid && newer->identity().nickname()=="Nowy" &&
        newer->profile()==*users::Profile::create(remaining) && value->identity().nickname()=="Żółw" && data.description=="Zażółć gęślą","Projection aliases/cancels incorrectly");
    const auto cleared=validator.validate(parse("BINF AAAB I4"),sid,adc::SessionHash::tiger192,&std::get<adc::ClientInfo>(update));
    require(adc::user_from_info(std::get<adc::ClientInfo>(cleared),sid)->profile()==users::Profile{},"Explicit address cancellation left endpoint metadata");
    auto bad=parse("BINF AAAB NINext DEChanged");bad.parameters[0]="NIe\xcc\x81";
    const auto invalid=validator.validate(bad,sid,adc::SessionHash::tiger192,&info);
    require(std::get<adc::InfError>(invalid)==adc::InfError::invalid_nick && adc::user_from_info(info,sid)==value,"ADC/User NFC policy differs or rejected update mutated source");
    // Users retains its generic 256-byte bound; ADC still enforces 64 bytes.
    const std::string wide(65,'N');
    require(users::Nickname::create(wide).has_value(),"Unexpected generic model restriction");
    const auto too_wide=validator.validate(parse("BINF AAAB NI"+wide),sid,adc::SessionHash::tiger192,&info);
    require(std::get<adc::InfError>(too_wide)==adc::InfError::field_limit,"ADC nickname bound relaxed");
}
}
int main() {
    try { checked_text();projection();std::cout<<"PASS: checked UTF-8/NFC user values, detached rename, registry rollback and verified public projection\n"; }
    catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
