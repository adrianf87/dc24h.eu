#include "dc24h/adc/msg.hpp"
#include "dc24h/adc/serializer.hpp"
#include "dc24h/adc/state.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace dc24h::adc;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
Message parsed(std::string_view text) {
    auto result=parse_msg(text);
    require(std::holds_alternative<Message>(result),"Valid MSG rejected");
    return std::get<Message>(std::move(result));
}
void semantic_error(std::string_view text, MsgError expected) {
    const auto result=parse_msg(text);
    require(std::get_if<MsgError>(&result) && std::get<MsgError>(result)==expected,"Incorrect MSG semantic error");
}
void syntax_error(std::string_view text, ParseError expected) {
    const auto result=parse_msg(text);
    require(std::get_if<ParseError>(&result) && std::get<ParseError>(result)==expected,"Incorrect MSG syntax error");
}
void examples_and_ownership() {
    for (const char* text : {"IMSG There\\sare\\s42\\susers\\sonline", "BMSG BBBB likes\\scats ME1",
        "DMSG CCCC BBBB I\\sprefer\\sdogs PMCCCC", "EMSG BBBB CCCC private PMBBBB ME0",
        "FMSG BBBB +BASE-TCP4 Hello", "HMSG hello", "BMSG BBBB PMCCCC", "IMSG enabled ME01", "IMSG disabled ME-0",
        "DMSG BBBB CCCC Group PMDDDD", "IMSG text XXone XXtwo X9future XX"}) {
        auto message=parsed(text);
        require(message.positional_count==1 && std::holds_alternative<MsgView>(validate_msg(message)),"Missing checked MSG layout");
        const auto wire=serialize_msg(message);
        require(std::get<std::string>(wire)==std::string(text)+'\n',"MSG round trip lost headers/flags/order");
        auto copied=message;
        message.parameters.clear();
        require(std::get<std::string>(serialize_msg(copied))==std::string(text)+'\n',"Parsed MSG borrowed input storage");
    }
    auto message=parsed("EMSG BBBB CCCC First\\sŻółw\\nsecond\\\\path PMDDDD ME0 X9future");
    const auto view=std::get<MsgView>(validate_msg(message));
    require(view.text=="First Żółw\nsecond\\path" && view.text.data()==message.parameters[0].data() &&
        view.private_group==Sid::parse("DDDD") && view.third_person==false,"Decoded borrowed MSG view or flags differ");
    const auto literal_message=parsed("IMSG ME1");
    const auto literal=std::get<MsgView>(validate_msg(literal_message));
    require(!literal.private_group && !literal.third_person,"Positional text treated as a flag");
    MsgOptions options;options.type=MessageType::echo;options.source=Sid::parse("BBBB");options.target=Sid::parse("CCCC");
    options.private_group=Sid::parse("BBBB");options.third_person=true;
    const auto built=make_msg("hello Żółw\n\\path",options);
    require(std::get<std::string>(serialize_msg(built))=="EMSG BBBB CCCC hello\\sŻółw\\n\\\\path PMBBBB ME1\n","Builder escaped or projected incorrectly");
    require(std::get<std::string>(serialize_msg(make_msg("Welcome")))=="IMSG Welcome\n","Default hub message builder");
    options.type=MessageType::feature;options.target.reset();options.features={{true,"BASE"},{false,"TCP4"}};
    require(std::get<std::string>(serialize_msg(make_msg("hello",options))).starts_with("FMSG BBBB +BASE-TCP4 hello"),"Feature header lost");
}
void failures() {
    for (const char* text : {"BMSG BBBB text PM", "BMSG BBBB text PMABC", "BMSG BBBB text PMABCDX",
                            "BMSG BBBB text PMABC0", "BMSG BBBB text PMabcd", "BMSG BBBB text PMBB\\sB"}) {
        semantic_error(text,MsgError::invalid_private_group);
    }
    for (const char* text : {"IMSG text ME", "IMSG text ME2", "IMSG text ME-1", "IMSG text MEtrue", "IMSG text ME9223372036854775808", "IMSG text ME+1", "IMSG text ME1\\n"}) {
        semantic_error(text,MsgError::invalid_third_person);
    }
    for (const char* text : {"IMSG text ME0 ME1", "DMSG BBBB CCCC text PMBBBB PMCCCC", "IMSG text ME1 ME1"}) {
        semantic_error(text,MsgError::duplicate_field);
    }
    semantic_error("CMSG hello",MsgError::invalid_type);semantic_error("UMSG ABCD hello",MsgError::invalid_type);
    semantic_error("HSCH ANhi",MsgError::invalid_message);semantic_error("",MsgError::invalid_message);
    syntax_error("IMSG",ParseError::missing_parameters);syntax_error("BMSG BBBB",ParseError::missing_parameters);
    syntax_error("DMSG BBBB CCCC",ParseError::missing_parameters);
    syntax_error("IMSG ",ParseError::invalid_separator);syntax_error("IMSG a  ME1",ParseError::invalid_separator);
    syntax_error("IMSG text bad",ParseError::invalid_named_parameter);
    syntax_error("IMSG bad\\q",ParseError::invalid_escape);syntax_error("IMSG bad\\",ParseError::invalid_escape);
    syntax_error("IMSG bad\n",ParseError::control_character);
    syntax_error(std::string("IMSG bad\0rest",13),ParseError::control_character);
    syntax_error(std::string("IMSG ")+char(0xc0)+char(0xaf),ParseError::invalid_utf8);
    syntax_error("IMSG e\xcc\x81",ParseError::non_normalized_text);
    auto message=parsed("BMSG BBBB text");
    for (int change=0;change<10;++change) {
        auto forged=message;
        switch(change) {
        case 0: forged.kind=Command::sta;break;
        case 1: forged.command="STA";break;
        case 2: forged.source="BBBB CCCC";break;
        case 3: forged.target="CCCC";break;
        case 4: forged.positional_count=0;break;
        case 5: forged.parameters[0].clear();break;
        case 6: forged.parameters[0]="bad\ttext";break;
        case 7: forged.parameters[0]="e\xcc\x81";break;
        case 8: forged.features={{true,"BASE"}};break;
        case 9: forged.type=static_cast<MessageType>('Z');break;
        }
        require(std::holds_alternative<MsgError>(validate_msg(forged)) &&
            std::holds_alternative<MsgError>(serialize_msg(forged)),"Forged MSG bypassed validation/serialization");
    }
    for (const char* text : {"", "bad\rtext", "e\xcc\x81"}) {
        bool failed=false;try { (void)make_msg(text); } catch (const std::invalid_argument&) { failed=true; }
        require(failed,"Builder accepted invalid local text");
    }
    MsgOptions bad;bad.type=MessageType::direct;bad.source=Sid::parse("BBBB");
    bool failed=false;try { (void)make_msg("hello",bad); } catch (const std::invalid_argument&) { failed=true; }
    require(failed,"Builder accepted incomplete direct header");
}
void boundaries_and_streams() {
    const auto exact=make_msg(std::string(max_frame_bytes-5,'x'));
    require(std::get<std::string>(serialize_msg(exact)).size()==max_frame_bytes+1,"Exact body/LF bound rejected");
    auto too_long=exact;too_long.parameters[0]+='x';
    require(std::get<MsgError>(serialize_msg(too_long))==MsgError::frame_too_long,"Oversized wire silently truncated");
    syntax_error("IMSG "+std::string(max_frame_bytes-4,'x'),ParseError::frame_too_long);
    auto escaped=make_msg(std::string((max_frame_bytes-6)/2,' ')+'x');
    require(std::get<std::string>(serialize_msg(escaped)).size()==max_frame_bytes+1,"Escaped exact wire bound rejected");
    escaped.parameters[0]+=' ';
    require(std::get<MsgError>(serialize_msg(escaped))==MsgError::frame_too_long,"Decoded/encoded bound confused");
    auto fields=parsed("IMSG hello");fields.parameters.resize(max_parameters,"XX");
    require(std::holds_alternative<std::string>(serialize_msg(fields)),"Exact parameter bound rejected");
    fields.parameters.emplace_back("XX");
    require(std::holds_alternative<MsgError>(serialize_msg(fields)),"Excess parameters accepted");
    StreamDecoder decoder;std::vector<Message> messages;unsigned invalid=0;
    const auto sink=[&](ParseResult result) {
        if (auto* message=std::get_if<Message>(&result)) {
            if (std::holds_alternative<MsgView>(validate_msg(*message))) { messages.push_back(std::move(*message)); }
            else { ++invalid; }
        } else if (std::holds_alternative<ParseError>(result)) { ++invalid; }
    };
    const auto wire=std::get<std::string>(serialize_msg(make_msg("Żółw 😀\n\\line")));
    for (const char c : wire) { decoder.feed(std::string_view(&c,1),sink); }
    decoder.feed("IMSG bad\\q\nIMSG bad ME2\n"+std::string(max_frame_bytes+1,'x')+"\n"+wire,sink);
    require(messages.size()==2 && invalid==3 && !decoder.finish(),"Fragmentation or malformed/oversize recovery failed");
    decoder.feed("IMSG unfinished",sink);require(decoder.finish()==ParseError::truncated_frame && !decoder.finish(),"Partial EOF accounting");
    decoder.feed(wire,sink);require(messages.size()==3,"Decoder did not recover after EOF");
}
void state_gates() {
    ConnectionStateMachine state;
    auto observe=[&](Direction direction,const char* wire) { return state.observe(direction,std::get<Message>(parse_line(wire))); };
    require(observe(Direction::from_client,"HMSG early")==StateError::invalid_state,"MSG before NORMAL accepted");
    require(!observe(Direction::from_client,"HSUP ADBASE ADTIGR") && !state.approve_protocol() &&
        !observe(Direction::to_client,"ISUP ADBASE ADTIGR") && !observe(Direction::to_client,"ISID BBBB") &&
        !observe(Direction::from_client,"BINF BBBB NIuser") && !state.approve_identity(IdentityMode::guest),"State fixture failed");
    require(observe(Direction::from_client,"BMSG CCCC text ME2")==StateError::wrong_source,"Source gate precedence changed");
    require(observe(Direction::from_client,"BMSG BBBB text ME2")==StateError::invalid_message && state.state()==ConnectionState::normal,"Invalid MSG changed state");
    require(!observe(Direction::from_client,"EMSG BBBB CCCC hi PMBBBB ME1") &&
        !observe(Direction::to_client,"IMSG Welcome"),"Valid MSG state gate failed");
}
int roundtrip_input() {
    StreamDecoder decoder;std::array<char,4096> bytes{};bool failed=false;unsigned frames=0;
    while (std::cin) {
        std::cin.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        decoder.feed(std::string_view(bytes.data(),static_cast<std::size_t>(std::cin.gcount())),[&](ParseResult result) {
            const auto* message=std::get_if<Message>(&result);
            if (!message) { failed=true;return; }
            const auto wire=serialize_msg(*message);
            if (const auto* text=std::get_if<std::string>(&wire)) { std::cout<<*text;++frames; }
            else { failed=true; }
        });
    }
    return failed || decoder.finish() || frames==0 ? 1 : 0;
}
}
int main(int argc,char* argv[]) {
    if (argc==2 && std::string_view(argv[1])=="--roundtrip") { return roundtrip_input(); }
    try { examples_and_ownership();failures();boundaries_and_streams();state_gates();
        std::cout<<"PASS: ADC MSG parsing, checked flags, escaping, wire bounds, ownership and state gates\n";
    } catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
