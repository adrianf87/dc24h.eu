#include "dc24h/adc/inf.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using namespace dc24h::adc;
constexpr auto in = Direction::from_client;
constexpr auto out = Direction::to_client;
void require(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
Message parse(std::string_view text) {
    auto result = parse_line(text);
    require(std::holds_alternative<Message>(result), "Invalid INF fixture syntax");
    return std::get<Message>(std::move(result));
}
Sid source() { return *Sid::parse("AAAB"); }
TigerDigest pid_bytes() {
    TigerDigest value{};
    for (std::size_t i=0; i<value.size(); ++i) { value[i] = static_cast<std::uint8_t>(i); }
    return value;
}
std::string cid() {
    const auto digest = tiger192(pid_bytes());
    require(digest.has_value(), "Tiger provider unavailable");
    return tiger_base32(*digest);
}
Message initial(std::string_view extra = {}) {
    return parse("BINF AAAB NIŻółw ID" + cid() + " PD" + tiger_base32(pid_bytes()) + std::string(extra));
}
ClientInfo validated(InfValidator& validator, const Message& message, const ClientInfo* previous=nullptr) {
    auto result = validator.validate(message, source(), SessionHash::tiger192, previous);
    require(std::holds_alternative<ClientInfo>(result), "Expected valid INF");
    return std::get<ClientInfo>(std::move(result));
}
void error(InfValidator& validator, const Message& message, InfError expected, const ClientInfo* previous=nullptr) {
    const auto result = validator.validate(message, source(), SessionHash::tiger192, previous);
    require(std::holds_alternative<InfError>(result) && std::get<InfError>(result)==expected, "Unexpected INF error");
}
void replace(Message& message, std::string_view name, std::string value) {
    const auto found = std::find_if(message.parameters.begin(), message.parameters.end(), [name](const auto& token) {
        return std::string_view(token).substr(0,2)==name;
    });
    require(found!=message.parameters.end(), "Missing replacement field");
    *found=std::string(name)+value;
}
std::string hex(const TigerDigest& value) {
    std::string output;
    for (const auto byte : value) { output += "0123456789ABCDEF"[byte>>4U]; output += "0123456789ABCDEF"[byte&15U]; }
    return output;
}
void hashes() {
    const auto empty=tiger192({});
    const std::array<std::uint8_t,3> abc{'a','b','c'};
    require(empty && hex(*empty)=="3293AC630C13F0245F92BBB1766E16167A4E58492DDE73F3", "Tiger empty vector/byte order");
    const auto hash=tiger192(abc);
    require(hash && hex(*hash)=="2AAB1484E8C158F2BFB8C5FF41B57A525129131C957B5F93", "Tiger abc vector/byte order");
    require(tiger_base32(TigerDigest{})==std::string(39,'A'), "Zero base32 vector");
    for (const auto* value : {"", "AAAA", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
                             "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB"}) {
        require(!parse_tiger_base32(value), "Noncanonical hash accepted");
    }
    std::uint32_t random=0x1234U;
    for (unsigned iteration=0; iteration<1000; ++iteration) {
        TigerDigest value{};
        for (auto& byte : value) { random=random*1664525U+1013904223U; byte=static_cast<std::uint8_t>(random>>24U); }
        auto encoded=tiger_base32(value);
        require(encoded.size()==39 && parse_tiger_base32(encoded)==value, "Base32 round trip");
        const auto alphabet=std::string_view("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
        encoded.back()=alphabet[alphabet.find(encoded.back())+1];
        require(!parse_tiger_base32(encoded), "Unused base32 bits accepted");
    }
}
void fields_and_updates() {
    InfValidator validator("127.0.0.1");
    auto info=validated(validator,initial(" DEHello\\sworld SS0 SF2 SL10 US1024 U465535 AW2 CT1 SUABC1,TCP4 APncdc VE1.0 EMx@y LOpl_PL XXignored"));
    require(info.field("NI")=="Żółw" && info.field("ID")==cid() && info.integer("SS")==0 &&
            info.integer("U4")==65535 && info.field("DE")=="Hello world", "Typed metadata");
    require(!info.field("PD") && !info.field("XX") && !info.integer("NI"), "Private/unknown retained");
    auto public_inf=info.public_message(source());
    require(public_inf.source=="AAAB" && public_inf.kind==Command::inf && public_inf.positional_count==0,
            "Public INF layout");
    for (const auto& field : public_inf.parameters) {
        require(field.substr(0,2)!="PD" && field.substr(0,2)!="XX", "Private/unknown exported");
    }
    public_inf.parameters.clear();
    require(info.field("NI")=="Żółw", "Public result aliases state");
    const auto updated=validated(validator,parse("BINF AAAB DE SS9223372036854775807 NIalice I40.0.0.0"),&info);
    require(!updated.field("DE") && updated.field("NI")=="alice" && updated.field("I4")=="127.0.0.1" &&
            updated.integer("SS")==std::numeric_limits<std::int64_t>::max(), "Incremental update/cancellation");
    require(info.field("DE")=="Hello world", "Input snapshot mutated");
    require(validated(validator,parse("BINF AAAB"),&updated)==updated, "Empty update must be a no-op");
    error(validator,parse("BINF AAAB NI"),InfError::missing_identity,&updated);
    error(validator,parse("BINF AAAB ID"),InfError::invalid_cid,&updated);
    error(validator,parse("BINF AAAB ID"+std::string(39,'A')),InfError::immutable_identity,&updated);
    error(validator,parse("BINF AAAB PD"),InfError::unexpected_pid,&updated);
    require(validated(validator,parse("BINF AAAB ID"+cid()),&updated)==updated, "Unchanged CID update");
    require(validated(validator,parse("BINF AAAB I4 SU CT"),&updated).field("I4")==std::nullopt,
            "Optional cancellation");
}
void failures() {
    InfValidator validator("127.0.0.1");
    error(validator,parse("BINF AAAB NIalice"),InfError::missing_identity);
    auto message=initial(); replace(message,"PD",""); error(validator,message,InfError::missing_identity);
    message=initial(); replace(message,"PD","AAAA"); error(validator,message,InfError::invalid_pid);
    message=initial(); replace(message,"ID","AAAA"); error(validator,message,InfError::invalid_cid);
    message=initial(); replace(message,"ID",std::string(39,'A')); error(validator,message,InfError::cid_mismatch);
    message=initial(); replace(message,"NI","a b"); error(validator,message,InfError::invalid_nick);
    message=initial(); replace(message,"NI",std::string(65,'a')); error(validator,message,InfError::field_limit);
    message=initial(); replace(message,"NI","e\xcc\x81"); error(validator,message,InfError::invalid_nick);
    message=initial(); replace(message,"NI",std::string("a\0b",3)); error(validator,message,InfError::invalid_nick);
    for (const auto* field : {"SS-1", "SS+1", "SS9223372036854775808", "SS1.0", "U465536", "U6-1", "AW3"}) {
        error(validator,initial(std::string(" ")+field),InfError::invalid_integer);
    }
    error(validator,initial(" CT4"),InfError::privilege_claim);
    error(validator,initial(" DEbad\\ntext"),InfError::invalid_text);
    for (const auto* field : {"SUA", "SUTCP4,", "SU,TCP4", "SUTCP4,TCP4", "SUbad1"}) {
        error(validator,initial(std::string(" ")+field),InfError::invalid_features);
    }
    error(validator,initial(" NIduplicate"),InfError::duplicate_field);
    error(validator,initial(" XXa XXb"),InfError::duplicate_field);
    error(validator,initial(" PDduplicate"),InfError::duplicate_field);
    error(validator,initial(" DE"+std::string(2048,'a')+" RF"+std::string(2048,'b')),InfError::field_limit);
    message=initial(); message.source="AAAC"; error(validator,message,InfError::wrong_source);
    message=initial(); message.type=MessageType::info; error(validator,message,InfError::invalid_message);
    message=initial(); message.target="AAAB"; error(validator,message,InfError::invalid_message);
    message=initial(); message.command="MSG"; error(validator,message,InfError::invalid_message);
    message=initial(); message.parameters.push_back("bad"); error(validator,message,InfError::invalid_message);
    message=initial(); message.parameters.resize(129,"XXx"); error(validator,message,InfError::invalid_message);
    require(validator.validate(initial(),source(),static_cast<SessionHash>(99))==
            std::variant<ClientInfo,InfError>{InfError::unsupported_hash}, "Unsupported hash fell back silently");
    require(inf_status_code(InfError::cid_mismatch)=="227" && inf_status_code(InfError::invalid_nick)=="221" &&
            inf_status_code(InfError::invalid_ip)=="246", "Fixed INF diagnostics");
}
void exact_boundaries() {
    InfValidator validator("127.0.0.1");
    auto message=initial(); replace(message,"NI",std::string(max_nick_bytes,'n'));
    require(validated(validator,message).field("NI")->size()==max_nick_bytes,"Exact nickname bound");
    std::string features;
    for (unsigned i=0; i<128; ++i) {
        const auto number=std::to_string(i);
        if (!features.empty()) { features+=','; }
        features+="Q"+std::string(3-number.size(),'0')+number;
    }
    require(validated(validator,initial(" SU"+features)).field("SU")->size()==639,"Exact feature bound");
    error(validator,initial(" SU"+features+",Q999"),InfError::invalid_features);
    const auto base=validated(validator,initial());
    const auto referral=max_info_bytes-base.bytes()-4-2048;
    const auto full=validated(validator,initial(" DE"+std::string(2048,'d')+" RF"+std::string(referral,'r')));
    require(full.bytes()==max_info_bytes,"Exact cumulative storage bound");
    error(validator,parse("BINF AAAB SS0"),InfError::field_limit,&full);
    error(validator,parse("BINF AAAB I40.0.0.0"),InfError::field_limit,&full);
    require(validated(validator,parse("BINF AAAB RF I40.0.0.0"),&full).field("I4")=="127.0.0.1",
            "Cancellation must free storage before IP normalization");
    message=initial(" I40.0.0.0"); replace(message,"I4",std::string("127.0.0.1\0evil",14));
    error(validator,message,InfError::invalid_ip);
}
void addresses() {
    for (const auto* peer : {"localhost", "127.0.0.1:1511", "[::1]", "fe80::1%eth0", ""}) {
        bool rejected=false;
        try { const InfValidator invalid(peer); } catch (const std::invalid_argument&) { rejected=true; }
        require(rejected,"Invalid trusted peer context accepted");
    }
    InfValidator v4("127.0.0.1"), v6("2001:db8::1"), mapped("::ffff:127.0.0.1");
    require(validated(v4,initial(" I40.0.0.0")).field("I4")=="127.0.0.1", "IPv4 zero substitution");
    require(validated(mapped,initial(" I4127.0.0.1")).field("I4")=="127.0.0.1", "Mapped peer normalization");
    require(validated(v6,initial(" I6::")).field("I6")=="2001:db8::1", "IPv6 zero substitution");
    require(validated(v6,initial(" I62001:0db8:0:0:0:0:0:1")).field("I6")=="2001:db8::1", "IPv6 canonicalization");
    for (const auto* field : {"I4127.0.0.2", "I4localhost", "I4256.0.0.1", "I6::", "I4127.0.0.1:42"}) {
        error(v4,initial(std::string(" ")+field),InfError::invalid_ip);
    }
    error(v6,initial(" I62001:db8::2"),InfError::invalid_ip);
    error(v6,initial(" I40.0.0.0"),InfError::invalid_ip);
}
void identify(ClientSession& session) {
    require(!session.observe(in,parse("HSUP ADBASE ADTIGR")),"Session SUP");
    require(!session.observe(out,*session.support().initial_reply()),"Session SUP send");
    require(!session.prepare_sid(),"Session reservation");
    require(!session.observe(out,*session.sid_reply()),"Session ISID send");
}
Message for_session(const ClientSession& session) {
    auto message=initial(" I40.0.0.0");
    message.source=std::string(session.sid());
    return message;
}
void sessions() {
    auto pool=std::make_shared<SidPool>(4);
    ClientSession guest(pool,"127.0.0.1");
    require(guest.approve_identity(IdentityMode::guest)==ClientSessionError{InfError::missing_identity},"Early admission");
    identify(guest);
    auto bad=for_session(guest); replace(bad,"ID",std::string(39,'A'));
    require(guest.observe(in,bad)==ClientSessionError{InfError::cid_mismatch} && !guest.info() && pool->size()==1 &&
            guest.state()==ConnectionState::identify,"Invalid INF changed state");
    require(!guest.observe(in,for_session(guest)) && guest.info() && guest.state()==ConnectionState::identify,
            "Validation must wait for external admission");
    require(!guest.approve_identity(IdentityMode::guest) && guest.state()==ConnectionState::normal,"Guest admission");
    const auto old=*guest.info();
    auto update=parse("BINF AAAB DEupdated SS-1"); update.source=std::string(guest.sid());
    require(guest.observe(in,update)==ClientSessionError{InfError::invalid_integer} && *guest.info()==old,
            "Failed update committed partial fields");
    update=parse("BINF AAAB DEupdated SS2"); update.source=std::string(guest.sid());
    require(!guest.observe(in,update) && guest.info()->field("DE")=="updated", "Normal update");
    auto leaked=guest.info()->public_message(*Sid::parse(guest.sid())); leaked.parameters.push_back("PDsecret");
    require(guest.observe(out,leaked)==ClientSessionError{InfError::unexpected_pid},"Outbound PID leak");
    require(!guest.observe(out,guest.info()->public_message(*Sid::parse(guest.sid()))),"Safe public export");

    ClientSession registered(pool,"127.0.0.1"); identify(registered);
    require(!registered.observe(in,for_session(registered)),"Registered INF");
    require(!registered.approve_identity(IdentityMode::password_required),"Registered decision");
    require(registered.observe(in,for_session(registered))==ClientSessionError{StateError::invalid_state},"INF in VERIFY");
    require(!registered.observe(out,parse("IGPA challenge")) && !registered.observe(in,parse("HPAS response")),"Password sequence");
    require(!registered.approve_password(),"Fixture password approval");
    require(!guest.observe(out,parse("IQUI "+std::string(guest.sid()))),"Own QUI");
    require(!guest.info() && pool->size()==1 && guest.approve_identity(IdentityMode::guest)==
            ClientSessionError{StateError::disconnected},"Terminal cleanup");
    registered.disconnect(); require(pool->size()==0 && !registered.info(),"Disconnect cleanup");
}
void stream_fragmentation() {
    const auto wire="BINF AAAB NIŻółw ID"+cid()+" PD"+tiger_base32(pid_bytes())+
        "\nBINF AAAB DEpartial SS-1\nBINF\nBINF AAAB DEGood\\sday\n";
    for (std::size_t split=0; split<=wire.size()+1; ++split) {
        auto pool=std::make_shared<SidPool>(1);
        ClientSession session(pool,"127.0.0.1"); identify(session);
        StreamDecoder decoder;
        unsigned accepted=0, rejected=0, invalid=0;
        const auto sink=[&](ParseResult result) {
            if (const auto* message=std::get_if<Message>(&result)) {
                if (session.observe(in,*message)) { ++rejected; }
                else {
                    ++accepted;
                    if (session.state()==ConnectionState::identify) {
                        require(!session.approve_identity(IdentityMode::guest),"Stream fixture admission");
                    }
                }
            } else if (std::holds_alternative<ParseError>(result)) { ++invalid; }
        };
        if (split==wire.size()+1) {
            for (const char byte : wire) { decoder.feed(std::string_view(&byte,1),sink); }
        } else {
            decoder.feed(std::string_view(wire).substr(0,split),sink);
            decoder.feed(std::string_view(wire).substr(split),sink);
        }
        require(!decoder.finish() && accepted==2 && rejected==1 && invalid==1 &&
                session.info()->field("DE")=="Good day" && !session.info()->field("PD"),
                "Fragmented INF changed semantics or recovery");
    }
}
void ncdc_transcript() {
    auto pool=std::make_shared<SidPool>(4);
    ClientSession session(pool,"127.0.0.1");
    std::string text;
    require(static_cast<bool>(std::getline(std::cin,text)),"Missing ncdc SUP");
    require(!session.observe(in,parse(text)),"ncdc SUP");
    std::cout<<*session.support().initial_reply_line()<<std::flush;
    require(static_cast<bool>(std::getline(std::cin,text)) && text=="SUP_SENT","Missing SUP completion");
    require(!session.observe(out,*session.support().initial_reply()) && !session.prepare_sid(),"ncdc SID prepare");
    std::cout<<*session.sid_reply_line()<<std::flush;
    require(static_cast<bool>(std::getline(std::cin,text)) && text=="SID_SENT","Missing SID completion");
    require(!session.observe(out,*session.sid_reply()),"ncdc SID completion");
    require(static_cast<bool>(std::getline(std::cin,text)),"Missing ncdc INF");
    const auto message=parse(text);
    require(!session.observe(in,message),"Actual ncdc INF validation failed");
    require(session.state()==ConnectionState::identify && session.info() && !session.info()->field("PD"),"Unexpected admission/PID retention");
    for (const auto& token : session.info()->public_message(*Sid::parse(session.sid())).parameters) {
        require(token.substr(0,2)!="PD","PID in public export");
    }
    session.disconnect(); require(pool->size()==0 && !session.info(),"ncdc fixture cleanup");
    std::cout<<"IDENTIFY INF_VALIDATED PD_REMOVED\n"<<std::flush;
}
} // namespace
int main(int argc,char** argv) {
    static_assert(!std::is_default_constructible_v<ClientInfo>);
    static_assert(!std::is_copy_constructible_v<ClientSession> && !std::is_move_constructible_v<ClientSession>);
    try {
        if (argc==2 && std::string_view(argv[1])=="--ncdc-transcript") { ncdc_transcript(); return 0; }
        require(argc==1,"Unexpected INF test arguments");
        hashes(); fields_and_updates(); failures(); exact_boundaries(); addresses(); sessions(); stream_fragmentation();
        std::cout<<"PASS: Tiger identity, bounded INF semantics, addresses, rollback and session gates\n";
    } catch (const std::exception& failure) { std::cerr<<"FAIL: "<<failure.what()<<'\n'; return 1; }
}
