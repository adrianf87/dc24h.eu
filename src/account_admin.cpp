#include "dc24h/api/accounts.hpp"
#include "dc24h/configuration/configuration.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/version.hpp"
#include <csignal>
#include <iostream>
#include <memory>
#include <stop_token>

int main(int argc,char* argv[]) {
    try {
        if(argc==2&&std::string_view(argv[1])=="--version"){std::cout<<"dc24h-account-admin "<<dc24h::version<<'\n';return 0;}
        const bool stage4 = argc==4 && std::string_view(argv[3])=="--stage4-serving";
        if((argc!=3&&!stage4)||std::string_view(argv[1])!="--config"){
            std::cerr<<"Usage: dc24h-account-admin --config maintenance.ini\nStop the serving hub before account maintenance. No ADC/ADCS listeners are opened.\n";return 2;
        }
        if(std::signal(SIGPIPE,SIG_IGN)==SIG_ERR){throw std::runtime_error("Cannot configure SIGPIPE handling");}
        auto settings=dc24h::configuration::parse_arguments(3,argv).settings;
        if(!settings.database.enabled||!settings.api.enabled||settings.authentication.enabled){
            throw std::runtime_error("Account maintenance requires database/API enabled and ADC authentication disabled");
        }
        // Validate storage/credentials before binding the administrative socket.
        { dc24h::database::Connection check(settings.database);const auto status=dc24h::database::inspect_migrations(check);
          if(status.dirty||status.pending||status.current_version!=dc24h::database::migration_catalog().size()){throw std::runtime_error("Account maintenance requires current database migrations");} }
        const dc24h::core::TerminationSignals signals;
        dc24h::logging::Logger logger(settings.logging);
        std::stop_source stop;
        settings.api.request_timeout=30;
        const auto public_settings=dc24h::configuration::public_settings(settings,true);
        const auto review=std::make_shared<dc24h::configuration::Review>(settings);
        dc24h::api::Server api(logger,settings.api,[public_settings]{dc24h::api::Snapshot s;s.database_enabled=true;s.configuration=public_settings;return s;},
            [&]{stop.request_stop();}, stage4 ? dc24h::api::stage4_account_handler(settings.database)
                                             : dc24h::api::account_handler(settings.database,review));
        api.start();logger.info("Account maintenance API ready; no hub listeners");
        (void)signals.wait(stop.get_token());return api.failed()?1:0;
    }catch(const std::exception&){std::cerr<<"Account maintenance startup failed\n";return 1;}
}
