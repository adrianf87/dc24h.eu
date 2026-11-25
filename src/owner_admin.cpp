#include "dc24h/admin/core.hpp"
#include "dc24h/configuration/configuration.hpp"
#include "dc24h/security/secret_file.hpp"
#include "dc24h/version.hpp"
#include <openssl/crypto.h>
#include <charconv>
#include <chrono>
#include <iostream>
#include <limits>
#include <map>

namespace {
struct Secret {
    std::string text;
    ~Secret() { OPENSSL_cleanse(text.data(),text.size()); }
};
std::uint64_t number(const std::string& text) {
    std::uint64_t value{};
    const auto [end,error] = std::from_chars(text.data(),text.data()+text.size(),value);
    if (error != std::errc{} || end != text.data()+text.size() || !value ||
        value > std::uint64_t(std::numeric_limits<std::int64_t>::max()) || std::to_string(value) != text) {
        throw std::runtime_error("Invalid input");
    }
    return value;
}
void metadata(std::string_view prefix, const dc24h::accounts::StoredAccount& account) {
    std::cout << prefix << "_id=" << account.account.id().value() << ' ' << prefix << "_revision=" << account.revision << '\n';
}
}
int main(int argc,char* argv[]) {
    using namespace dc24h;
    auto language = i18n::Language::us;
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--version") { std::cout << "dc24h-twilight-owner " << version << '\n'; return 0; }
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "Stop every serving hub and maintenance process first. No listeners or migrations.\n"
                "dc24h-twilight-owner --config maintenance.ini --bootstrap NAME --password-file FILE --confirm-stopped\n"
                "dc24h-twilight-owner --config maintenance.ini --transfer OWNER_ID --revision REV --to ADMIN_ID --to-revision REV --password-file FILE --target-password-file FILE --confirm-stopped\n"
                "Passwords must be in private same-UID regular files (0600), never command arguments.\n"; return 0;
        }
        std::map<std::string,std::string> options; bool confirmed = false;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--confirm-stopped") { if (confirmed) { throw std::runtime_error("Duplicate option"); } confirmed = true; continue; }
            if (key != "--config" && key != "--bootstrap" && key != "--password-file" && key != "--transfer" &&
                key != "--revision" && key != "--to" && key != "--to-revision" && key != "--target-password-file") { throw std::runtime_error("Unknown option"); }
            if (++i == argc || !options.emplace(key,argv[i]).second) { throw std::runtime_error("Invalid option"); }
        }
        const bool bootstrap = options.contains("--bootstrap");
        if (!confirmed || options.size() != (bootstrap ? 3U : 7U)) { throw std::runtime_error("Invalid options"); }
        char* config_argv[]{argv[0],const_cast<char*>("--config"),options.at("--config").data()};
        const auto settings = configuration::parse_arguments(3,config_argv).settings;
        language = settings.logging.language;
        if (!settings.database.enabled || settings.authentication.enabled) { throw std::runtime_error("Invalid maintenance configuration"); }
        // Validate all IDs/flags and read both protected files before connecting.
        const auto owner_id = bootstrap ? 0 : number(options.at("--transfer"));
        const auto owner_revision = bootstrap ? 0 : number(options.at("--revision"));
        const auto target_id = bootstrap ? 0 : number(options.at("--to"));
        const auto target_revision = bootstrap ? 0 : number(options.at("--to-revision"));
        Secret proof{security::read_private_secret(options.at("--password-file"))};
        Secret target{bootstrap ? std::string{} : security::read_private_secret(options.at("--target-password-file"))};
        admin::Engine engine(settings.database,admin::Origin::owner_cli);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        admin::Command command;command.time=now;
        if (bootstrap) {
            command.action=admin::Action::bootstrap_owner;command.name=options.at("--bootstrap");command.password=proof.text;
            const auto result = std::get<accounts::StoredAccount>(engine.execute(command));
            std::cout << i18n::translate("Owner provisioning completed",language) << '\n'; metadata("owner",result);
        } else {
            command.action=admin::Action::transfer_owner;command.actor={owner_id,owner_revision};command.target={target_id,target_revision};
            command.proof=proof.text;command.successor_proof=target.text;
            const auto result = std::get<accounts::OwnershipTransfer>(engine.execute(command));
            std::cout << i18n::translate("Ownership transfer completed",language) << '\n';
            metadata("former_owner",result.former_owner); metadata("owner",result.new_owner);
        }
        return 0;
    } catch (const admin::Error& error) {
        if (error.kind()==admin::ErrorKind::outcome_unknown) { std::cerr << i18n::translate("Ownership outcome unknown; inspect storage before retrying",language) << '\n'; return 3; }
    } catch (...) {}
    std::cerr << i18n::translate("Ownership operation refused",language) << '\n'; return 1;
}
