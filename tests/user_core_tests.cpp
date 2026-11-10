#include "dc24h/users/session.hpp"
#include "dc24h/adc/login.hpp"
#include "dc24h/adc/identity_hash.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace dc24h;
using namespace std::chrono_literals;
void require(bool value,const char* reason) { if (!value) { throw std::runtime_error(reason); } }
users::User user(std::string_view sid="AAAA",std::string_view cid="CID",std::string_view nick="Guest",users::Profile profile={}) {
    return *users::User::create(*users::Identity::create(sid,cid,nick),std::move(profile));
}
void model() {
    users::ProfileData data;
    data.description=std::string(2048,'D');data.email=std::string(254,'E');
    data.application=std::string(128,'A');data.version=std::string(128,'V');data.locale=std::string(64,'L');
    data.shared_bytes=std::numeric_limits<std::int64_t>::max();data.shared_files=0;data.slots=42;
    for (unsigned i=0;i<128;++i) { data.features.push_back("F"+std::to_string(100+i)); }
    const auto maximum=users::Profile::create(data);
    require(maximum.has_value() && maximum->data()==data,"Exact profile bounds rejected");
    const auto original=data;
    for (auto member : {&users::ProfileData::description,&users::ProfileData::email,&users::ProfileData::application,&users::ProfileData::version,&users::ProfileData::locale}) {
        data=original;(data.*member).push_back('X');require(!users::Profile::create(data),"Oversize profile text accepted");
        data=original;(data.*member)="a\nb";require(!users::Profile::create(data),"Profile control accepted");
        data=original;(data.*member)=std::string("a\0b",3);require(!users::Profile::create(data),"Profile NUL accepted");
    }
    for (auto member : {&users::ProfileData::shared_bytes,&users::ProfileData::shared_files,&users::ProfileData::slots}) {
        data=original;data.*member=-1;require(!users::Profile::create(data),"Negative declaration accepted");
    }
    for (const auto& bad : {std::vector<std::string>{"ABCD","ABCD"}, {"abcD"}, {"ABCD","A?00"}, {"AAA"}}) {
        data=original;data.features=bad;require(!users::Profile::create(data),"Malformed/duplicate feature accepted");
    }
    data=original;data.features.push_back("AAAA");require(!users::Profile::create(data),"Feature cap ignored");
    users::Session session(*users::SessionId::from_value(1),users::Transport::adcs,{});
    auto candidate=user("AAAA","CID","Żółw",*maximum);
    require(session.identify(candidate,{})==users::SessionError::invalid_state && !session.user(),"User attached before negotiation");
    require(!session.start_negotiation({}) && !session.identify(candidate,users::TimePoint{}+1ms),"User admission failed");
    const auto saved=session.snapshot();
    require(saved.profile==maximum && session.user()->profile()==*maximum,"Profile not retained");
    require(session.update_user(user("AAAB"),users::TimePoint{}+2ms)==users::SessionError::invalid_identity && session.snapshot()==saved,"SID rebound");
    require(session.update_user(user("AAAA","Other"),users::TimePoint{}+2ms)==users::SessionError::invalid_identity && session.snapshot()==saved,"CID rebound");
    require(session.update_user(user(),{})==users::SessionError::backwards_time && session.snapshot()==saved,"Backwards profile update mutated model");
    require(!session.mark_ready(users::TimePoint{}+2ms),"Ready failed");
    require(!session.update_user(user("AAAA","CID","Changed"),users::TimePoint{}+3ms),"Atomic nickname/profile replacement failed");
    require(session.identity()->nickname()=="Changed" && session.user()->profile().data().description.empty() &&
        saved.identity->nickname()=="Żółw" && saved.profile->data().description.size()==2048,"Snapshot/update aliases source");
    require(!session.rename("Renamed",users::TimePoint{}+4ms) && session.user()->identity().nickname()=="Renamed","Legacy rename bypassed user");
    require(!session.begin_close(users::CloseCause::shutdown,users::TimePoint{}+5ms),"Close failed");
    const auto closing=session.snapshot();
    require(session.update_user(candidate,users::TimePoint{}+6ms)==users::SessionError::invalid_state && session.snapshot()==closing,"Closing user mutated");
    require(!session.finish_close(users::CloseCause::shutdown,users::TimePoint{}+7ms),"Finish failed");
    require(session.user()->identity().nickname()=="Renamed" && saved.profile==maximum,"Closure discarded profile history");
}
adc::Message parse(std::string_view line) { return std::get<adc::Message>(adc::parse_line(line)); }
void drain(adc::LoginHub& hub,std::uint64_t id) {
    while (!hub.output(id).empty() && !hub.must_close(id)) { (void)hub.consume(id,hub.output(id).size()); }
}
std::string negotiate(adc::LoginHub& hub,std::uint64_t id) {
    hub.connect(id,"127.0.0.1");hub.receive(id,parse("HSUP ADBASE ADTIGR"));
    (void)hub.consume(id,hub.output(id).size());const std::string sid(hub.output(id).substr(5,4));drain(hub,id);return sid;
}
adc::Message login(std::string sid,unsigned char seed,std::string_view nick) {
    adc::TigerDigest pid{};pid[0]=seed;
    return parse("BINF "+sid+" ID"+adc::tiger_base32(*adc::tiger192(pid))+" PD"+adc::tiger_base32(pid)+" NI"+std::string(nick)+
        " DEDescription EMpublic@example.test APClient VE1.2 LOpl_PL SS9223372036854775807 SF10 SL3 SUTCP4,ADC0 I40.0.0.0 XXignored");
}
void adapter() {
    adc::LoginHub hub;
    const auto first=negotiate(hub,1);hub.receive(1,login(first,1,"First"));
    const auto identified=*hub.snapshot(1);
    require(identified.phase==users::Phase::identified && !hub.ready(1) && identified.profile.has_value(),"User admission conflated with readiness");
    const auto& profile=identified.profile->data();
    require(profile.description=="Description" && profile.email=="public@example.test" && profile.application=="Client" && profile.version=="1.2" &&
        profile.locale=="pl_PL" && profile.shared_bytes==std::numeric_limits<std::int64_t>::max() && profile.shared_files==10 && profile.slots==3 &&
        profile.features==std::vector<std::string>{"TCP4","ADC0"},"Public profile projection wrong/truncated");
    drain(hub,1);require(hub.ready(1),"Ready failed");
    hub.receive(1,parse("BINF "+first+" NIRenamed DEChanged EM SS SF SL SU AP VE LO"));drain(hub,1);
    const auto updated=*hub.snapshot(1);
    require(updated.identity->nickname()=="Renamed" && updated.profile->data().description=="Changed" && updated.profile->data().email.empty() &&
        !updated.profile->data().shared_bytes && updated.profile->data().features.empty() && updated.profile->data().application.empty(),"Merged INF cancellation left stale profile");
    require(identified.identity->nickname()=="First" && profile.description=="Description","User snapshot aliases update");
    // Validation must reject the complete candidate, preserving the prior public model.
    hub.receive(1,parse("BINF "+first+" DEBad SS-1"));
    require(hub.snapshot(1)->profile==updated.profile && hub.snapshot(1)->identity==updated.identity,"Rejected INF partially committed user profile");
    hub.disconnect(1);
    require(!hub.snapshot(1) && identified.profile->data().description=="Description","User copy failed after cleanup");
    const auto second=negotiate(hub,2);hub.receive(2,login(second,2,"Second"));drain(hub,2);
    const auto second_user=*hub.snapshot(2);
    const auto third=negotiate(hub,3);hub.receive(3,login(third,3,"Third"));drain(hub,3);
    const auto third_before=*hub.snapshot(3);
    hub.receive(3,parse("BINF "+third+" NISecond DEUncommitted"));
    require(hub.closing(3) && hub.snapshot(3)->identity==third_before.identity && hub.snapshot(3)->profile==third_before.profile,"Duplicate nick modified user");
    hub.disconnect(3);require(hub.ready(2),"Rejected peer damaged survivor");hub.disconnect(2);
    const auto reconnect=negotiate(hub,4);hub.receive(4,login(reconnect,2,"Second"));drain(hub,4);
    require(hub.ready(4) && hub.snapshot(4)->identity->cid()==second_user.identity->cid() && hub.snapshot(4)->id!=second_user.id,"Reconnect/public identity failed");hub.disconnect(4);
}
}
int main() {
    try { model();adapter();std::cout<<"PASS: public user/profile bounds, lifecycle, atomic updates, INF projection/cancellation and cleanup\n"; }
    catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
