#include "dc24h/users/session_manager.hpp"
#include "dc24h/adc/login.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace dc24h::users;
void require(bool value, const char* reason) { if (!value) { throw std::runtime_error(reason); } }
SessionId id(std::uint64_t value) { return *SessionId::from_value(value); }
Session& add(SessionManager& manager, std::uint64_t value, Transport transport=Transport::adc) {
    return *std::get<Session*>(manager.add(id(value),transport,TimePoint{}));
}
void registry() {
    static_assert(!std::is_copy_constructible_v<SessionManager> && !std::is_move_constructible_v<SessionManager>);
    for (auto capacity : {std::size_t{0}, SessionManager::max_capacity+1}) {
        bool rejected=false;
        try { SessionManager manager(capacity); } catch (const std::invalid_argument&) { rejected=true; }
        require(rejected,"Invalid registry capacity accepted");
    }
    SessionManager manager(2);
    auto& first=add(manager,1,Transport::adcs);
    auto& second=add(manager,3);
    require(std::get<RegistryError>(manager.add(id(3),Transport::adc,{}))==RegistryError::stale_id,"Duplicate accepted");
    require(std::get<RegistryError>(manager.add(id(4),Transport::adc,{}))==RegistryError::capacity_reached,"Capacity ignored");
    require(!manager.find(id(2)) && !manager.snapshot(id(2)),"Missing ID found");
    require(manager.remove(id(1))==RegistryError::not_closed,"Live session erased");
    require(!first.start_negotiation({}) && !first.identify(*Identity::create("AAAA","CID","First"),{}) && !first.mark_ready({}),"Setup failed");
    require(!second.start_negotiation({}),"Second negotiation failed");
    const auto before=manager.counts();
    require(before.total==2 && before.ready==1 && before.negotiating==1,"Phase counts drift");
    const auto page=manager.page(0,1);
    require(page.size()==1 && page[0].id==id(1) && manager.page(1,1)[0].id==id(3),"Cursor gaps/order lost");
    require(manager.page(std::numeric_limits<std::uint64_t>::max()).empty(),"End cursor wrapped");
    for (auto limit : {std::size_t{0}, SessionManager::max_page_size+1}) {
        bool rejected=false;try { (void)manager.page(0,limit); } catch (const std::invalid_argument&) { rejected=true; }
        require(rejected,"Unbounded page allowed");
    }
    require(!first.rename("Changed",{}) && page[0].identity->nickname()=="First","Page aliases live model");
    require(!first.begin_close(CloseCause::shutdown,{}),"Close failed");
    require(manager.counts().closing==1 && manager.counts().ready==0,"Closing counted as ready");
    require(!first.finish_close(CloseCause::peer_closed,{}) && manager.counts().closed==1,"Closed count missing");
    require(!manager.remove(id(1)) && !manager.snapshot(id(1)),"Closed entry retained");
    require(manager.remove(id(1))==RegistryError::missing,"Double removal ambiguous");
    require(std::get<RegistryError>(manager.add(id(1),Transport::adc,{}))==RegistryError::stale_id,"Removed ID reused");
    // Failed capacity registration did not commit an ID or entry.
    auto& fourth=add(manager,4);
    require(manager.find(id(3))==&second && manager.find(id(4))==&fourth,"Stable references moved");
    require(page[0].identity->nickname()=="First","Removed snapshot lost ownership");
}
void churn() {
    SessionManager manager(300);
    auto* retained=&add(manager,1);
    for (std::uint64_t value=2;value<=300;++value) { (void)add(manager,value); }
    require(manager.page(0,256).size()==256 && manager.page(256,256).size()==44,"Large page bounds");
    for (std::uint64_t value=2;value<=300;++value) {
        require(!manager.find(id(value))->finish_close(CloseCause::peer_closed,{}),"Churn close");
        require(!manager.remove(id(value)),"Churn remove");
    }
    for (std::uint64_t value=301;value<=2000;++value) {
        auto& model=add(manager,value);
        require(!model.finish_close(CloseCause::shutdown,{}) && !manager.remove(id(value)),"Reconnect churn");
    }
    require(manager.find(id(1))==retained && manager.counts().total==1 && manager.counts().accepted==1,"Churn damaged survivor");
    SessionManager final(1);auto& last=add(final,std::numeric_limits<std::uint64_t>::max());
    require(!last.finish_close(CloseCause::shutdown,{}) && !final.remove(last.id()),"Maximum ID cleanup");
    require(std::get<RegistryError>(final.add(id(1),Transport::adc,{}))==RegistryError::stale_id,"Watermark wrapped");
}
void live_adapter() {
    dc24h::adc::LoginHub hub({2,1024,4096});
    hub.connect(1,"127.0.0.1",Transport::adcs);hub.connect(2,"127.0.0.1");
    auto count=hub.sessions().counts();
    require(count.total==2 && count.accepted==1 && count.negotiating==1,"Live transports missing from registry");
    const auto saved=hub.sessions().page();
    hub.transport_ready(1);hub.close(2,dc24h::adc::CloseReason::login_timeout);
    count=hub.sessions().counts();
    require(count.negotiating==1 && count.closing==1,"Live lifecycle differs from registry");
    require(&hub.session_model(1)==hub.sessions().find(id(1)),"Duplicate session owner");
    hub.disconnect(2);hub.disconnect(1,false,CloseCause::shutdown);hub.disconnect(1);
    require(hub.sessions().size()==0 && hub.sessions().page().empty() && saved.size()==2,"Live cleanup left registry entries");
    hub.connect(3,"127.0.0.1");
    require(!hub.sessions().snapshot(id(1)) && hub.sessions().size()==1,"Stale lookup reached replacement");
    hub.disconnect(3);
    bool invalid=false;
    try { hub.connect(4,"invalid-address"); } catch (const std::invalid_argument&) { invalid=true; }
    require(invalid && hub.sessions().size()==0,"Protocol setup failure left orphan model");
    bool reused=false;
    try { hub.connect(4,"127.0.0.1"); } catch (const std::logic_error&) { reused=true; }
    require(reused,"Failed setup allowed ID reuse");
    hub.connect(5,"127.0.0.1");hub.disconnect(5);
}
}
int main() {
    try { registry();churn();live_adapter();std::cout<<"PASS: registry ownership, limits, phases, pagination, churn and live adapter\n"; }
    catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n';return 1; }
}
