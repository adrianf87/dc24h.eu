#include "dc24h/chat/settings.hpp"
#include "dc24h/adc/chat_validation.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace dc24h;
using namespace dc24h::adc;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
template<class F> void invalid(F function) {
    bool rejected=false; try { function(); } catch (const std::invalid_argument&) { rejected=true; }
    require(rejected,"Invalid chat setting accepted");
}
users::SessionId id(std::uint64_t value) { return *users::SessionId::from_value(value); }
Message parse(std::string_view value) { return std::get<Message>(parse_line(value)); }
void add(users::SessionManager& sessions,std::uint64_t owner,std::string_view sid) {
    require(std::holds_alternative<const users::Session*>(sessions.add(id(owner),users::Transport::adc,{})) &&
        !sessions.start_negotiation(id(owner),{}) && !sessions.identify(id(owner),
        *users::User::create(*users::Identity::create(sid,"C"+std::to_string(owner),"Nick"+std::to_string(owner))),{}) &&
        !sessions.mark_ready(id(owner),{}),"Session setup failed");
}
std::string drain(LoginHub& hub,std::uint64_t owner) {
    std::string result;
    while (!hub.output(owner).empty() && !hub.must_close(owner)) { const std::string wire(hub.output(owner)); result+=wire; hub.consume(owner,wire.size()); }
    return result;
}
std::string join(LoginHub& hub,std::uint64_t owner,std::string& welcome) {
    hub.connect(owner,"127.0.0.1"); hub.receive(owner,parse("HSUP ADBASE ADTIGR"));
    hub.consume(owner,hub.output(owner).size()); const std::string sid(hub.output(owner).substr(5,4)); hub.consume(owner,hub.output(owner).size());
    TigerDigest pid{}; pid[0]=static_cast<std::uint8_t>(owner);
    hub.receive(owner,parse("BINF "+sid+" NIUser"+std::to_string(owner)+" ID"+tiger_base32(*tiger192(pid))+" PD"+tiger_base32(pid)));
    welcome=drain(hub,owner); require(hub.ready(owner),"Configured welcome prevented readiness"); return sid;
}
void settings_and_policy() {
    chat::Settings defaults; chat::validate_settings(defaults);
    require(defaults.policy.public_enabled && defaults.policy.private_enabled && defaults.policy.actions_enabled &&
        defaults.welcome_enabled && defaults.welcome_message == hub_welcome_text && defaults.policy.max_text_bytes == 16384 &&
        defaults.flood.messages_per_second == 5 && defaults.flood.message_burst == 10,"Defaults changed existing behavior");
    auto settings=defaults; settings.policy.max_text_bytes=0; invalid([&] { chat::validate_settings(settings); });
    settings.policy.max_text_bytes=16385; invalid([&] { chat::validate_settings(settings); });
    settings=defaults; settings.welcome_message=""; invalid([&] { chat::validate_settings(settings); });
    settings.welcome_enabled=false; chat::validate_settings(settings);
    settings.welcome_message="e\xcc\x81"; invalid([&] { chat::validate_settings(settings); });
    settings=defaults; settings.welcome_message=std::string(4096,' '); chat::validate_settings(settings);
    settings.welcome_message+=' '; invalid([&] { chat::validate_settings(settings); });
    users::SessionManager sessions(2); add(sessions,1,"AAAB"); add(sessions,2,"AAAC"); chat::Core core(sessions);
    chat::Policy policy; policy.max_text_bytes=4;
    MessageRouter router(core,policy);
    for (const auto text : {"abcd","😀","    "}) {
        auto message=parse("BMSG AAAB placeholder"); message.parameters[0]=text;
        require(std::holds_alternative<RoutedMessage>(router.prepare(id(1),message)),"Exact decoded policy limit rejected or whitespace trimmed");
    }
    for (const auto text : {"abcde","😀x","Żółw"}) {
        auto message=parse("EMSG AAAB AAAC placeholder PMAAAB"); message.parameters[0]=text;
        require(std::get<RoutingError>(router.prepare(id(1),message)) == RoutingError::text_limit,"Decoded UTF-8 bytes not enforced");
        require(std::get<PrivateChatError>(PrivateChat(core,policy).prepare(id(1),message)) == PrivateChatError::text_limit,"Direct private adapter bypassed policy");
    }
    policy={}; policy.public_enabled=false;
    require(std::get<PublicChatError>(PublicChat(core,policy).prepare(id(1),parse("BMSG AAAB Text"))) == PublicChatError::disabled &&
        std::holds_alternative<PrivateDelivery>(PrivateChat(core,policy).prepare(id(1),parse("DMSG AAAB AAAC Text PMAAAB"))),"Public/private switches not independent");
    policy={}; policy.private_enabled=false;
    require(std::holds_alternative<PublicBroadcast>(PublicChat(core,policy).prepare(id(1),parse("BMSG AAAB Text"))) &&
        std::get<PrivateChatError>(PrivateChat(core,policy).prepare(id(1),parse("EMSG AAAB AAAC Text PMAAAB"))) == PrivateChatError::disabled,"Private switch leaked echo");
    policy={}; policy.actions_enabled=false;
    require(std::get<RoutingError>(MessageRouter(core,policy).prepare(id(1),parse("EMSG AAAB AAAC Text ME01 PMAAAB"))) == RoutingError::disabled &&
        std::holds_alternative<RoutedMessage>(MessageRouter(core,policy).prepare(id(1),parse("BMSG AAAB Text ME0"))),"Action alias bypassed switch or ME0 was blocked");
    const PublicChat snapshot(core,policy); policy.public_enabled=false;
    require(std::holds_alternative<PublicBroadcast>(snapshot.prepare(id(1),parse("BMSG AAAB Text"))),"Adapter borrowed mutable startup settings");
    require(std::get<PublicChatError>(PublicChat(core,policy).prepare(id(2),parse("BMSG AAAB Text"))) == PublicChatError::wrong_source,"Policy masked source ownership");
    policy.max_text_bytes=0; invalid([&] { MessageRouter bad(core,policy); });
}
void welcome_and_runtime() {
    LoginLimits limits{.welcome_text=std::nullopt}; limits.chat.public_enabled=false; limits.chat.private_enabled=false;
    LoginHub disabled(limits); std::string welcome;
    const auto first=join(disabled,1,welcome);
    require(welcome.starts_with("IINF ") && welcome.find("IMSG ") == std::string::npos && welcome.ends_with("\n"),"Disabled welcome still queued system text");
    const auto second=join(disabled,2,welcome); (void)drain(disabled,1);
    disabled.receive(1,parse("BMSG "+first+" SecretDisabled"));
    require(drain(disabled,1) == "ISTA 125 Chat\\soperation\\sis\\sdisabled FCBMSG\n" && disabled.output(2).empty() && disabled.ready(1),"Disabled public chat published or closed source");
    disabled.receive(1,parse("EMSG "+first+" "+second+" SecretDisabled PM"+first));
    require(drain(disabled,1).starts_with("ISTA 125 ") && disabled.output(2).empty(),"Disabled private chat leaked a copy");
    require(std::get<chat::DispatchReport>(disabled.send_hub_message("System remains enabled")).queued == 2,"Client switches suppressed trusted system output");
    (void)drain(disabled,1); (void)drain(disabled,2); disabled.disconnect(1,false); disabled.disconnect(2,false);
    require(disabled.queued_bytes() == 0,"Disabled-policy cleanup leaked charges");
    limits={}; limits.chat.max_text_bytes=4; limits.welcome_text="CFGSecret Żółw\\n PMAAAB ME1";
    LoginHub custom(limits); const auto source=join(custom,1,welcome);
    const auto expected=std::get<SharedHubMessageFrame>(make_hub_message(*limits.welcome_text))->wire();
    require(welcome.ends_with(expected) && expected.find("\\\\n") != std::string::npos &&
        expected.find(" PM") == std::string::npos,"Custom welcome was interpreted as flags/escapes or subject to client length");
    const auto target=join(custom,2,welcome); (void)drain(custom,1);
    custom.receive(1,parse("EMSG "+source+" "+target+" abcde PM"+source));
    require(drain(custom,1) == "ISTA 100 Chat\\stext\\slimit\\sexceeded FCEMSG\n" && custom.output(2).empty() && custom.ready(1),"Configured length denial not recoverable/private");
    custom.receive(1,parse("BMSG "+source+" 😀"));
    require(drain(custom,1) == "BMSG "+source+" 😀\n" && drain(custom,2) == "BMSG "+source+" 😀\n","Length recovery failed");
    custom.disconnect(1,false); custom.disconnect(2,false); require(custom.queued_bytes() == 0,"Configured queue charges leaked");
    limits.welcome_text=""; invalid([&] { LoginHub bad(limits); });
}
}
int main() {
    try { settings_and_policy(); welcome_and_runtime(); std::cout << "PASS: checked chat settings, immutable policy, recoverable mode/text limits and optional/custom welcome\n"; }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
