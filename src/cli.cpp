#include "dc24h/cli.hpp"

#include "dc24h/version.hpp"
#include "server_cli.hpp"

#include <ostream>

namespace dc24h {

int run_cli(std::span<const std::string_view> arguments, std::ostream &output,
            std::ostream &errors) {
    if (arguments.empty()) {
        output << project_name << " — ADC/ADCS Hub — " << release_label << '\n';
        return 0;
    }
    if (arguments.front() == "--serve") {
        return run_server(arguments.subspan(1), output, errors);
    }

    if (arguments.size() != 1) {
        errors << "dc24h-hub: expected at most one argument.\n";
    } else if (arguments.front() == "--help") {
        output << "Usage: dc24h-hub [--help | --version]\n";
        write_server_help(output);
        output << "  --help     Show this help.\n"
               << "  --version  Show the project version.\n";
        return 0;
    } else if (arguments.front() == "--version") {
        output << project_name << ' ' << release_label << " (" << version << ")\n";
        return 0;
    } else {
        errors << "dc24h-hub: unknown argument '" << arguments.front() << "'.\n";
    }

    errors << "Try 'dc24h-hub --help' for usage.\n";
    return 2;
}

} // namespace dc24h
