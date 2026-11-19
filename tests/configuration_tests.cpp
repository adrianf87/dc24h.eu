#include "dc24h/configuration/configuration.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/security/moderation.hpp"

#include <unistd.h>
#include <sys/stat.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>

namespace {
namespace fs = std::filesystem;
void require(bool value, const char* name) {
    if (!value) { throw std::runtime_error(name); }
}
template<class F> void invalid(F function) {
    bool rejected = false;
    try { function(); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "invalid configuration was accepted");
}
struct Fixture {
    fs::path dir;
    Fixture() {
        char buffer[] = "/tmp/dc24h-conf-XXXXXX";
        const char* path = ::mkdtemp(buffer);
        require(path != nullptr, "mkdtemp failed");
        dir = path;
    }
    ~Fixture() { std::error_code ec; fs::remove_all(dir, ec); }
    fs::path write(std::string_view content, std::string_view name = "hub.ini") {
        const auto target = dir / std::string(name);
        std::ofstream stream(target, std::ios::binary);
        stream << content;
        require(static_cast<bool>(stream), "Cannot write fixture");
        return target;
    }
};
void load_and_override() {
    Fixture fx;
    const auto path = fx.write(
        "[network]\nlisten = 127.0.0.1:411\nlisten = [::1]:0\n"
        "adcs_listen = 127.0.0.1:1512\nmax_connections = 42\nidle_timeout = 6\n"
        "[tls]\ncertificate = certs/chain.pem\nprivate_key = certs/secret.key\n"
        "[logging]\nlevel = error\nfile = logs/hub.log\nchat = on\n");
    const auto loaded = dc24h::configuration::load_file(path.string());
    require(loaded.network.listeners.size() == 2, "Plain listeners missing");
    require(loaded.network.tls_listeners.size() == 1, "TLS listener missing");
    require(loaded.network.listeners[0].port == 411, "Listen port invalid");
    require(loaded.network.max_connections == 42, "Connection limit invalid");
    require(loaded.network.idle_timeout.count() == 6, "Timeout invalid");
    require(loaded.network.tls_certificate == (fx.dir / "certs/chain.pem").string(),
            "Relative certificate path not resolved against INI directory");
    require(loaded.logging.chat, "INI chat logging missing");
    require(loaded.logging.level == dc24h::logging::Level::error, "Log level invalid");
    require(loaded.logging.file_path == (fx.dir / "logs/hub.log").string(),
            "Relative log path not resolved against INI directory");

    auto str = path.string();
    std::vector<std::string> options{"dc24h", "--max-connections", "19", "--config", str,
                                      "--listen", "127.0.0.1:0", "--log-file", "-",
                                      "--log-level", "info", "--log-chat", "off"};
    std::vector<char*> argv;
    for (auto& item : options) { argv.push_back(item.data()); }
    const auto parsed = dc24h::configuration::parse_arguments(
        static_cast<int>(argv.size()), argv.data());
    require(parsed.settings.network.listeners.size() == 1, "CLI listeners did not replace file listeners");
    require(parsed.settings.network.listeners.front().port == 0, "CLI listener port invalid");
    require(parsed.settings.network.max_connections == 19, "CLI does not win over configuration");
    require(parsed.settings.logging.file_path.empty(), "CLI stderr override failed");
    require(!parsed.settings.logging.chat, "CLI chat logging override missing");
    require(parsed.settings.logging.level == dc24h::logging::Level::info, "CLI log level override failed");
}

void invalid_configs() {
    Fixture fx;
    for (const auto& content : {
        "[network]\nlisten = localhost:411\n",
        "[network]\nlisten = 127.0.0.1:2\nlisten = 127.0.0.1:2\n",
        "[network]\nmax_connections = 0\n",
        "[network]\nidle_timeout = 86401\n",
        "[logging]\nlevel = debug\n",
        "[logging]\nchat = yes\n",
        "[logging]\nchat = on\nchat = off\n",
        "[logging]\nlevel = info\nlevel = error\n",
        "[unknown]\nkey = value\n",
        "[tls]\nunknown = value\n",
        "[network]\nlisten = none\n",
        "[network]\nlisten = none\nlisten = 127.0.0.1:1\n",
        "[network]\nlisten = 127.0.0.1:1\nlisten = none\n",
        "[network]\nlisten = 127.0.0.1:1\nadcs_listen = 127.0.0.1:1\n",
        "[network]\nadcs_listen = 127.0.0.1:1\n",
    }) {
        const auto path = fx.write(content);
        // load_file deliberately parses fields; merged validity is checked separately.
        invalid([&] {
            const auto parsed = dc24h::configuration::load_file(path.string());
            dc24h::configuration::validate(parsed);
        });
    }
    const auto oversized = fx.write(std::string(65537, 'x'));
    invalid([&] { (void)dc24h::configuration::load_file(oversized.string()); });
    const auto good = fx.write("[network]\nlisten = 127.0.0.1:0\n");
    const auto symlink = fx.dir / "linked.ini";
    fs::create_symlink(good, symlink);
    invalid([&] { (void)dc24h::configuration::load_file(symlink.string()); });
}

dc24h::configuration::CommandLine parse(std::vector<std::string> options) {
    std::vector<char*> argv;
    for (auto& option : options) { argv.push_back(option.data()); }
    return dc24h::configuration::parse_arguments(static_cast<int>(argv.size()), argv.data());
}

void boundaries_and_precedence() {
    Fixture fx;
    const auto bom = fx.write("\xEF\xBB\xBF[general]\r\nlanguage=pl\r\n[network]\r\nlisten=127.0.0.1:0");
    auto settings = dc24h::configuration::load_file(bom.string());
    dc24h::configuration::validate(settings);
    require(settings.logging.language == dc24h::i18n::Language::pl, "File language missing");
    auto parsed = parse({"dc24h", "--language", "us", "--config", bom.string(), "--check-config"});
    require(parsed.settings.logging.language == dc24h::i18n::Language::us, "CLI language lost to file");
    std::string maximum;
    for (int i = 0; i < 16; ++i) { maximum += "#" + std::string(4094, 'x') + "\n"; }
    require(maximum.size() == 65536, "Invalid boundary fixture");
    (void)dc24h::configuration::load_file(fx.write(maximum).string());
    (void)dc24h::configuration::load_file(fx.write("#" + std::string(4095, 'x')).string());
    for (const auto& content : {std::string("#") + std::string(4096, 'x'), std::string("#\xff"),
                                std::string("#x\0y", 4), std::string("#a\rb\n")}) {
        invalid([&] { (void)dc24h::configuration::load_file(fx.write(content).string()); });
    }
    invalid([&] { (void)parse({"dc24h", "--config", bom.string(), "--config", bom.string()}); });
    const auto incomplete = fx.write("[network]\nadcs_listen=127.0.0.1:0\n[tls]\ncertificate=chain.pem\n");
    parsed = parse({"dc24h", "--config", incomplete.string(), "--tls-key", "key.pem"});
    require(parsed.settings.network.listeners.empty(), "TLS-only file exposed plaintext");
    require(parsed.settings.network.tls_private_key == "key.pem", "CLI path must stay relative to cwd");
    parsed = parse({"dc24h", "--adcs-listen", "127.0.0.1:0", "--tls-cert", "cert.pem", "--tls-key", "key.pem"});
    require(parsed.settings.network.listeners.empty(), "TLS-only CLI exposed plaintext");
    const auto duplicate = fx.write("[network]\nlisten=[::1]:411\nlisten=[0:0:0:0:0:0:0:1]:411\n");
    invalid([&] { (void)parse({"dc24h", "--config", duplicate.string()}); });
    invalid([&] { (void)parse({"dc24h", "--listen", "none", "--check-config"}); });
    std::vector<std::string> too_many{"dc24h"};
    for (int i = 0; i < 17; ++i) { too_many.insert(too_many.end(), {"--listen", "127.0.0.1:0"}); }
    invalid([&] { (void)parse(too_many); });
}

void user_limits() {
    Fixture fx;
    require(dc24h::i18n::help(dc24h::i18n::Language::us).find("--max-users")!=std::string_view::npos &&
        dc24h::i18n::help(dc24h::i18n::Language::pl).find("--max-sessions")!=std::string_view::npos,
        "User limit CLI help missing");
    require(dc24h::i18n::translate("User session limit reached; excess peer rejected",dc24h::i18n::Language::pl)==
        "Osiągnięto limit sesji użytkowników; nadmiarowy klient odrzucony","Session limit warning lacks Polish translation");
    const auto path=fx.write("[network]\nmax_connections=8\n[users]\nmax_sessions=4\nmax_users=2\n");
    auto parsed=parse({"dc24h","--config",path.string(),"--check-config"});
    require(parsed.settings.network.users==dc24h::users::Limits{4,2},"INI user limits missing");
    parsed=parse({"dc24h","--max-users","1","--config",path.string(),"--max-sessions","3"});
    require(parsed.settings.network.users==dc24h::users::Limits{3,1},"CLI user limits did not override INI");
    parsed=parse({"dc24h","--max-connections","1","--max-sessions","65535","--max-users","65535"});
    require(parsed.settings.network.users.bounded_by(parsed.settings.network.max_connections)==dc24h::users::Limits{1,1},
        "Effective limits raised lower network ceiling");
    for (const auto* key : {"max_sessions","max_users"}) {
        for (const auto* value : {"0","65536","-1","+1","1x","18446744073709551616",""}) {
            const auto bad=fx.write("[users]\n"+std::string(key)+"="+value+"\n");
            invalid([&] { (void)parse({"dc24h","--config",bad.string()}); });
        }
        const auto duplicate=fx.write("[users]\n"+std::string(key)+"=1\n"+key+"=2\n");
        invalid([&] { (void)parse({"dc24h","--config",duplicate.string()}); });
    }
    for (const auto* option : {"--max-users","--max-sessions"}) {
        invalid([&] { (void)parse({"dc24h",option}); });
        for (const auto* value : {"0","65536","-1","+1","1x","18446744073709551616"}) {
            invalid([&] { (void)parse({"dc24h",option,value}); });
        }
    }
    dc24h::configuration::Settings settings;
    settings.network.users.max_users=0;
    invalid([&] { dc24h::configuration::validate(settings); });
    settings.network.users={65536,1};
    invalid([&] { dc24h::configuration::validate(settings); });
}

void logger_file() {
    Fixture fx;
    const auto path = fx.dir / "events.log";
    {
        dc24h::logging::Logger log({dc24h::logging::Level::error, path.string()});
        log.info("NEVER_EMIT");
        log.error("SAMPLE_ERROR");
    }
    struct stat s{};
    require(::stat(path.c_str(), &s) == 0, "log does not exist");
    require((s.st_mode & 0777) == 0600, "new log file must be private");
    {
        std::ifstream input(path);
        std::string value((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        require(value.find("SAMPLE_ERROR") != std::string::npos, "log severity not emitted");
        require(value.find("NEVER_EMIT") == std::string::npos, "filtered INFO was emitted");
    }
    {
        dc24h::logging::Logger log({dc24h::logging::Level::off, path.string()});
        log.info("OFF_INFO"); log.error("OFF_ERROR");
    }
    {
        dc24h::logging::Logger log({dc24h::logging::Level::info, path.string()});
        std::vector<std::jthread> writers;
        for (int writer = 0; writer < 4; ++writer) {
            writers.emplace_back([&log, writer] {
                for (int i = 0; i < 20; ++i) { log.info("THREAD_" + std::to_string(writer) + "_" + std::to_string(i)); }
            });
        }
    }
    {
        std::ifstream input(path);
        std::string line;
        int lines = 0;
        while (std::getline(input, line)) {
            require(line.find("OFF_") == std::string::npos, "Off logger emitted a message");
            require(line.find("[ERROR] SAMPLE_ERROR") != std::string::npos ||
                    line.find("[INFO] THREAD_") != std::string::npos, "Log lines interleaved");
            ++lines;
        }
        require(lines == 81, "Append/concurrent logging lost records");
    }
    const auto symlink = fx.dir / "linked.log";
    fs::create_symlink(path, symlink);
    bool blocked = false;
    try { dc24h::logging::Logger log({dc24h::logging::Level::info, symlink.string()}); }
    catch (const std::exception&) { blocked = true; }
    require(blocked, "symlink output must be rejected");
}
void chat_limits() {
    Fixture fx;
    require(dc24h::i18n::translate("Invalid chat policy",dc24h::i18n::Language::pl) == "Niepoprawna polityka czatu" &&
        dc24h::i18n::translate("Invalid chat welcome message",dc24h::i18n::Language::pl) == "Niepoprawna wiadomość powitalna czatu", "Chat settings errors lack Polish translations");
    require(dc24h::i18n::translate("Invalid chat anti-flood limits",dc24h::i18n::Language::pl) ==
        "Niepoprawne limity anti-flood czatu", "Chat limit error lacks Polish translation");
    const auto path=fx.write("[chat]\nmax_messages_per_second=7\nmessage_burst=12\nmax_bytes_per_second=65536\nbyte_burst=262144\n");
    const auto settings=parse({"dc24h","--config",path.string(),"--check-config"}).settings;
    require(settings.network.chat.flood.messages_per_second == 7 && settings.network.chat.flood.message_burst == 12 &&
        settings.network.chat.flood.bytes_per_second == 65536 && settings.network.chat.flood.byte_burst == 262144, "INI chat limits not propagated");
    for (const auto key : {"max_messages_per_second","message_burst","max_bytes_per_second","byte_burst"}) {
        for (const auto value : {"0","-1","+1","18446744073709551616","999999999",""}) {
            const auto bad=fx.write("[chat]\n"+std::string(key)+"="+value+"\n");
            invalid([&] { (void)parse({"dc24h","--config",bad.string()}); });
        }
        const auto duplicate=fx.write("[chat]\n"+std::string(key)+"=16384\n"+key+"=16384\n");
        invalid([&] { (void)parse({"dc24h","--config",duplicate.string()}); });
    }
    const auto too_small=fx.write("[chat]\nbyte_burst=16383\n");
    invalid([&] { (void)parse({"dc24h","--config",too_small.string()}); });
    const auto policy=fx.write("[chat]\nmax_text_bytes=512\npublic_enabled=off\nprivate_enabled=on\nactions_enabled=off\nwelcome_enabled=on\nwelcome_message=Witaj Żółw PMAAAA ME1\\n\n");
    const auto configured=parse({"dc24h","--config",policy.string(),"--check-config"}).settings.network.chat;
    require(configured.policy.max_text_bytes == 512 && !configured.policy.public_enabled && configured.policy.private_enabled &&
        !configured.policy.actions_enabled && configured.welcome_enabled && configured.welcome_message == "Witaj Żółw PMAAAA ME1\\n", "Chat policy/welcome INI not preserved");
    for (const auto key : {"public_enabled","private_enabled","actions_enabled","welcome_enabled"}) {
        for (const auto value : {"","true","false","1","ON"}) {
            const auto bad=fx.write("[chat]\n"+std::string(key)+"="+value+"\n");
            invalid([&] { (void)parse({"dc24h","--config",bad.string()}); });
        }
        const auto duplicate=fx.write("[chat]\n"+std::string(key)+"=on\n"+key+"=off\n");
        invalid([&] { (void)parse({"dc24h","--config",duplicate.string()}); });
    }
    for (const auto value : {"0","16385","-1","18446744073709551616"}) {
        const auto bad=fx.write("[chat]\nmax_text_bytes="+std::string(value)+"\n");
        invalid([&] { (void)parse({"dc24h","--config",bad.string()}); });
    }
    for (const auto& value : {std::string(),std::string("e\xcc\x81"),std::string(4097,'x')}) {
        const auto bad=fx.write("[chat]\nwelcome_message="+value+"\n");
        invalid([&] { (void)parse({"dc24h","--config",bad.string()}); });
    }
    const auto no_welcome=fx.write("[chat]\nwelcome_enabled=off\nwelcome_message=\n");
    require(!parse({"dc24h","--config",no_welcome.string()}).settings.network.chat.welcome_enabled, "Explicitly disabled empty welcome refused");
}
void chat_welcome_ini_bounds() {
    Fixture fx;
    std::string unicode;
    for (int i=0; i<1024; ++i) { unicode += "😀"; }
    for (const auto& welcome : {std::string(4081,'x'),std::string(4096,'x'),unicode}) {
        for (const auto ending : {"\n","\r\n",""}) {
            const auto path=fx.write("[chat]\nwelcome_message="+welcome+ending);
            require(parse({"dc24h","--config",path.string(),"--check-config"}).settings.network.chat.welcome_message == welcome,
                "Full-size welcome rejected or changed by INI line accounting");
        }
    }
    // The extended physical-line cap includes whitespace, the key and optional CR.
    const std::string maximum_line=std::string(4077,' ')+"welcome_message = "+std::string(4096,'x')+'\r';
    require(maximum_line.size() == 8192,"Invalid welcome line boundary fixture");
    const auto path=fx.write("[chat]\n"+maximum_line+'\n');
    require(parse({"dc24h","--config",path.string()}).settings.network.chat.welcome_message == std::string(4096,'x'),
        "Padded maximum welcome line rejected");
    for (const auto& content : {
            "[chat]\n "+maximum_line+'\n',
            "[chat]\nwelcome_message="+std::string(4097,'x')+'\n',
            "[chat]\nwelcome_enabled=off\nwelcome_message="+std::string(4097,'x')+'\n',
            "[chat]\nwelcome_message_extra="+std::string(4096,'x')+'\n',
            "[logging]\nwelcome_message="+std::string(4096,'x')+'\n',
            "[chat]\n#"+std::string(4096,'x')+'\n',
            "[chat]\n"+std::string(4097,' ')+'\n',
            "[chat]\nwelcome_message="+unicode+"\nwelcome_message=Duplicate\n"}) {
        const auto bad=fx.write(content);
        invalid([&] { (void)parse({"dc24h","--config",bad.string()}); });
    }
}
} // namespace

int main() {
    try {
        {
            dc24h::configuration::Settings settings;
            auto defaults=dc24h::configuration::public_settings(settings);
            require(defaults.connect_timeout_seconds==5&&defaults.read_timeout_seconds==5&&defaults.require_tls,"Safe defaults differ");
            settings.database.timeout=8;settings.database.read_timeout=2;settings.database.write_timeout=9;
            settings.database.name="PrivateDatabaseName";settings.database.user="PrivateDatabaseUser";
            settings.database.socket="/tmp/private-database.sock";settings.database.password_file="private-password-file";
            settings.authentication.key_file="private-adc-file";
            const auto safe=dc24h::configuration::public_settings(settings);
            require(safe.connect_timeout_seconds==8&&safe.read_timeout_seconds==2&&safe.write_timeout_seconds==9,"Timeout inheritance differs");
            const auto text=dc24h::api::configuration_json(safe);
            require(text.find("Private")==std::string::npos&&text.find("private-")==std::string::npos,"Public settings leak paths/credentials");
            std::string moderation="\"moderation\":[";
            bool first_rule=true;
            for(auto operation:dc24h::security::moderation_catalog){
                const auto rule=dc24h::security::moderation_rule(operation);
                if(!first_rule){moderation+=',';}first_rule=false;
                moderation+="{\"operation\":\"";moderation+=dc24h::security::moderation_operation_key(operation);
                moderation+="\",\"permission\":\"";moderation+=dc24h::security::permission_key(rule.permission);
                moderation+="\",\"subject\":\"";moderation+=dc24h::security::moderation_subject_key(rule.subject);
                moderation+="\",\"target\":\"";moderation+=dc24h::security::moderation_target_key(rule.target);
                moderation+="\",\"guest_target\":";moderation+=rule.guest_target?"true":"false";
                moderation+=",\"forbid_self\":";moderation+=rule.forbid_self?"true":"false";
                moderation+=",\"vip_delegable\":";moderation+=rule.vip_delegable?"true":"false";
                moderation+=",\"requires_blocks\":";moderation+=rule.requires_blocks?"true":"false";
                moderation+='}';
            }
            moderation+="]";
            require(text.find(moderation)!=std::string::npos,"Public moderation rules differ");
            for(auto capacity:{std::size_t{0},std::size_t{4097}}){settings.authentication.max_accounts=capacity;invalid([&]{dc24h::configuration::validate(settings);});}
            settings.authentication.max_accounts=4096;
            settings.authentication.enabled=true;invalid([&]{dc24h::configuration::validate(settings);});
            settings.database.enabled=true;settings.authentication.require_tls=false;dc24h::configuration::validate(settings);
            invalid([&]{(void)dc24h::configuration::public_settings(settings,true);});
            settings.authentication.enabled=false;
            require(dc24h::configuration::public_settings(settings,true).maintenance,"Maintenance mode missing");
        }
        {
            Fixture fixture;const auto path=fixture.write("[roles]\nvip_permissions=users.kick,audit.read\n");
            const auto loaded=dc24h::configuration::load_file(path.string());
            require(dc24h::security::allows(dc24h::accounts::Role::vip,dc24h::security::Permission::revoke_sessions,loaded.network.roles),"VIP configuration lost");
            for(const auto text:{"vip_permissions=accounts.manage","vip_permissions=*","vip_permissions=users.kick,users.kick","vip_permissions=none\nvip_permissions=none"}){
                fixture.write(std::string("[roles]\n")+text+"\n");invalid([&]{(void)dc24h::configuration::load_file(path.string());});
            }
        }
        load_and_override();
        invalid_configs();
        boundaries_and_precedence();
        user_limits();
        chat_limits();
        chat_welcome_ini_bounds();
        logger_file();
        std::cout << "Configuration parsing, CLI overrides, limits and safe logging OK\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
