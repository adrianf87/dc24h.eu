#include "dc24h/logging/logger.hpp"
#include <sys/stat.h>
#include <cerrno>
#include <unistd.h>
#include <utf8proc.h>
#include <chrono>
#include <thread>
#include <vector>
#include <regex>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace { bool fail_log_writes = false; }
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t bytes) {
    if (fail_log_writes) { errno=ENOSPC; return -1; }
    return __real_write(fd,data,bytes);
}

namespace {
namespace fs = std::filesystem;
using Event = dc24h::logging::ConnectionEvent;
using Language = dc24h::i18n::Language;
void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
struct Fixture {
    fs::path dir;
    Fixture() {
        char name[] = "/tmp/dc24h-logging-XXXXXX";
        const char* path = ::mkdtemp(name);
        require(path != nullptr, "mkdtemp failed");
        dir = path;
    }
    ~Fixture() { std::error_code ec; fs::remove_all(dir, ec); }
    std::string contents(std::string_view name) const {
        std::ifstream file(dir / std::string(name), std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }
};
std::size_t occurrences(const std::string& text, std::string_view value) {
    std::size_t count = 0;
    for (std::size_t at = text.find(value); at != std::string::npos;
         at = text.find(value, at + value.size())) { ++count; }
    return count;
}
void severity_and_sanitization() {
    Fixture f;
    const auto name = (f.dir / "events.log").string();
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::info, name, Language::us, false});
        logger.info("Startup successful");
        logger.warning("Memory pressure\nFAKE_EVENT");
        logger.error("Test error");
        logger.info(std::string(2000, 'X'));
        logger.connection(Event::accepted, 1, false);
    }
    const auto result = f.contents("events.log");
    require(result.find("[INFO] Startup successful") != std::string::npos, "missing info");
    require(result.find("[WARN] Memory pressure?FAKE_EVENT") != std::string::npos,
            "newline injection not sanitized");
    require(result.find("[ERROR] Test error") != std::string::npos, "missing error");
    require(result.find("...[truncated]") != std::string::npos, "line length not limited");
    require(result.find("Connection id=") == std::string::npos, "connection event not opt-in");
    require(occurrences(result, "FAKE_EVENT") == 1, "log injection produced multiple records");
    struct stat state{};
    require(::stat(name.c_str(), &state) == 0, "log missing");
    require((state.st_mode & 0777) == 0600, "new file must have 0600 permissions");
}
void levels() {
    Fixture f;
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::warning,
                                      (f.dir / "warning.log").string(), Language::us, true});
        logger.info("SKIP_INFO");
        logger.connection(Event::accepted, 11, false);
        logger.warning("KEEP_WARN");
        logger.error("KEEP_ERROR");
    }
    const auto warning = f.contents("warning.log");
    require(warning.find("KEEP_WARN") != std::string::npos, "warning not emitted");
    require(warning.find("KEEP_ERROR") != std::string::npos, "error not emitted");
    require(warning.find("SKIP_INFO") == std::string::npos, "info not suppressed");
    require(warning.find("Connection id=") == std::string::npos, "peer log bypassed level");
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::error,
                                      (f.dir / "error.log").string(), Language::us, false});
        logger.warning("SKIP_WARN");
        logger.error("KEEP_ERROR");
    }
    const auto error = f.contents("error.log");
    require(error.find("SKIP_WARN") == std::string::npos, "warning not suppressed");
    require(error.find("KEEP_ERROR") != std::string::npos, "error not emitted");
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::off,
                                      (f.dir / "off.log").string(), Language::us, true});
        logger.info("SKIP");
        logger.warning("SKIP");
        logger.error("SKIP");
        logger.connection(Event::accepted, 2, true);
    }
    require(f.contents("off.log").empty(), "off level emitted output");
}
void peer_events_and_budget() {
    Fixture f;
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::info,
                                      (f.dir / "peers.log").string(), Language::us, true});
        require(logger.connection_enabled(), "connection logger disabled");
        for (std::uint64_t i = 0; i < 120; ++i) {
            logger.connection(Event::accepted, i, (i % 2) != 0);
        }
        logger.connection(Event::peer_closed, 121, true);
    }
    const auto result = f.contents("peers.log");
    require(occurrences(result, "event=accepted") == 64, "rate limit count mismatch");
    require(result.find("transport=ADCS") != std::string::npos, "encrypted event missing");
    require(result.find("transport=ADC") != std::string::npos, "plain event missing");
    require(result.find("[WARN] Connection log events suppressed=57") != std::string::npos,
            "dropped event summary missing");
    require(result.find("event=peer_closed") == std::string::npos, "budget bypassed");
}
void unicode_bounds_and_single_records() {
    Fixture f;
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::info, (f.dir / "unicode.log").string()});
        logger.info(std::string("CTRL\n\r\t\x1b\x7f\0", 10));
        logger.info("Unicode Cześć 😀");
        logger.info("Unicode format: \xc2\x85\xe2\x80\xa8\xe2\x80\xa9\xe2\x80\xae");
        logger.info("Invalid \xff\xc0\xaf");
        logger.info(std::string(1024, 'A'));
        std::string emoji;
        for (int i = 0; i < 300; ++i) { emoji += "😀"; }
        logger.info(emoji);
        logger.info(std::string(1024 * 1024, 'X'));
    }
    const auto data = f.contents("unicode.log");
    require(occurrences(data, "\n") == 7, "message generated an extra/missing record");
    require(data.find("Cześć 😀") != std::string::npos, "Unicode text was damaged");
    require(data.find("Unicode format: ????") != std::string::npos, "Unicode controls not removed");
    require(data.find("Invalid ???") != std::string::npos, "invalid encoding not sanitized");
    require(occurrences(data, "...[truncated]") == 2, "truncation boundary wrong");
    std::ifstream input(f.dir / "unicode.log");
    std::string line;
    const std::regex prefix("^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z \\[INFO\\] ");
    while (std::getline(input, line)) {
        std::smatch matched;
        require(std::regex_search(line, matched, prefix), "missing UTC severity prefix");
        require(line.size() - matched.length() <= 1024, "sanitized body exceeds 1024 bytes");
        for (std::size_t i = 0; i < line.size();) {
            utf8proc_int32_t cp{};
            const auto count = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(line.data() + i),
                static_cast<utf8proc_ssize_t>(line.size() - i), &cp);
            require(count > 0, "record contains invalid or truncated UTF-8");
            const auto category = utf8proc_category(cp);
            require(category != UTF8PROC_CATEGORY_CC && category != UTF8PROC_CATEGORY_CF &&
                    category != UTF8PROC_CATEGORY_ZL && category != UTF8PROC_CATEGORY_ZP,
                    "record contains a Unicode control/separator");
            i += static_cast<std::size_t>(count);
        }
    }
}

void window_rollover_and_polish() {
    Fixture f;
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::info,
                                      (f.dir / "window.log").string(), Language::pl, true});
        for (std::uint64_t i = 0; i < 65; ++i) { logger.connection(Event::accepted, i, false); }
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        logger.connection(Event::tls_ready, 100, true);
        logger.warning("Network idle connection closed");
        logger.warning("Security negotiation limit reached; connection closed");
        logger.warning("Security identity verification limit reached; connection closed");
    }
    const auto data = f.contents("window.log");
    require(occurrences(data, "event=accepted") == 64, "wrong first-window budget");
    require(occurrences(data, "event=tls_ready") == 1, "new window did not admit events");
    require(occurrences(data, "suppressed=1") == 1, "rollover/destructor duplicated the summary");
    require(data.find("Limit negocjacji przekroczony") != std::string::npos &&
            data.find("Limit weryfikacji tożsamości przekroczony") != std::string::npos, "Negotiation warnings not localized");
    require(data.find("Połączenie id=") != std::string::npos, "connection prefix not localized");
    require(data.find("[WARN] Zamknięto bezczynne połączenie") != std::string::npos,
            "warning not localized");
}

void concurrent_peer_budget() {
    Fixture f;
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::info,
                                      (f.dir / "threads.log").string(), Language::us, true});
        std::vector<std::jthread> writers;
        for (int thread = 0; thread < 4; ++thread) {
            writers.emplace_back([&logger, thread] {
                for (int i = 0; i < 30; ++i) {
                    logger.connection(Event::accepted, static_cast<std::uint64_t>(thread * 30 + i), false);
                }
            });
        }
    }
    const auto data = f.contents("threads.log");
    require(occurrences(data, "event=accepted") == 64, "concurrent callers bypassed rate budget");
    require(occurrences(data, "suppressed=56") == 1, "concurrent suppression count was lost");
    require(occurrences(data, "\n") == 65, "concurrent records interleaved");
}

void chat_budget_levels_and_localization() {
    Fixture f;
    for (const auto level : {dc24h::logging::Level::info, dc24h::logging::Level::warning,
                             dc24h::logging::Level::error, dc24h::logging::Level::off}) {
        const auto name="chat"+std::to_string(static_cast<int>(level))+".log";
        {
            dc24h::logging::Logger logger({level,(f.dir/name).string(),Language::us,true,true});
            logger.chat_event(static_cast<dc24h::chat::Event>(999));
            std::vector<std::jthread> threads;
            for (int thread=0;thread<4;++thread) {
                threads.emplace_back([&logger] {
                    for (int i=0;i<30;++i) { logger.chat_event(dc24h::chat::Event::private_dispatch); }
                });
            }
            for (auto& thread:threads) { thread.join(); }
            logger.connection(Event::accepted,1,false); // Independent budget.
        }
        const auto text=f.contents(name);
        if (level==dc24h::logging::Level::info) {
            require(occurrences(text,"Chat event=private_dispatch")==32,"Concurrent chat budget bypassed");
            require(occurrences(text,"Chat log events suppressed=88")==1,"Chat suppression count missing");
            require(occurrences(text,"event=accepted")==1,"Chat consumed connection budget");
        } else { require(text.empty(),"Chat logging bypassed severity"); }
    }
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::info,(f.dir/"default.log").string()});
        logger.chat_event(dc24h::chat::Event::rejected);
    }
    require(f.contents("default.log").empty(),"Chat logging not opt-in");
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::info,(f.dir/"chat-pl.log").string(),Language::pl,false,true});
        for (int i=0;i<33;++i) { logger.chat_event(dc24h::chat::Event::rejected); }
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        logger.chat_event(dc24h::chat::Event::public_dispatch);
    }
    {
        dc24h::logging::Logger logger({dc24h::logging::Level::info,(f.dir/"failed.log").string(),Language::us,false,true});
        fail_log_writes=true;
        logger.chat_event(dc24h::chat::Event::private_dispatch);
        fail_log_writes=false;
        logger.chat_event(dc24h::chat::Event::public_dispatch);
    }
    require(f.contents("failed.log").find("Chat event=public_dispatch")!=std::string::npos &&
        f.contents("failed.log").find("private_dispatch")==std::string::npos,"Log sink failure stopped subsequent events");
    const auto pl=f.contents("chat-pl.log");
    require(occurrences(pl,"Czat zdarzenie=rejected")==32 && occurrences(pl,"Czat zdarzenie=public_dispatch")==1 &&
        occurrences(pl,"Pominięte zdarzenia logu czatu=1")==1,"Chat localization/window rollover differs");
}

}
int main() {
    if (dc24h::i18n::translate("Users stopped; online=0", dc24h::i18n::Language::pl) !=
        "Użytkownicy zakończeni; online=0") { return 1; }
    try {
        severity_and_sanitization();
        levels();
        peer_events_and_budget();
        unicode_bounds_and_single_records();
        window_rollover_and_polish();
        concurrent_peer_budget();
        chat_budget_levels_and_localization();
        std::cout << "Logging levels, sanitization, connection events and rate budget OK\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
