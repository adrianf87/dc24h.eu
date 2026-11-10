#include "dc24h/network/tcp_server.hpp"
#include "dc24h/core/server.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"

#include <sys/socket.h>
#include <signal.h>
#include <iostream>
#include <thread>

// Executable-local fault fixture: production accept4 and socket tuning are
// untouched. A tiny server send buffer forces real TCP/SSL write backpressure.
extern "C" int __real_accept4(int, sockaddr*, socklen_t*, int);
extern "C" int __wrap_accept4(int fd, sockaddr* address, socklen_t* length, int flags) {
    const int client=__real_accept4(fd,address,length,flags);
    if (client>=0) {
        const int capacity=1024;
        (void)::setsockopt(client,SOL_SOCKET,SO_SNDBUF,&capacity,sizeof(capacity));
    }
    return client;
}
int main(int argc, char** argv) {
    try {
        if (argc!=3) { throw std::invalid_argument("Expected TLS certificate/key"); }
        ::signal(SIGPIPE,SIG_IGN);
        const dc24h::core::TerminationSignals signals;
        dc24h::logging::Options logs; logs.connections=true;
        dc24h::logging::Logger logger(logs);
        dc24h::network::Options options;
        options.listeners={{"127.0.0.1",0}};options.tls_listeners={{"127.0.0.1",0}};
        options.tls_certificate=argv[1];options.tls_private_key=argv[2];
        options.idle_timeout=std::chrono::seconds(2);
        options.security.close_timeout=std::chrono::seconds(1);
        options.security.output_timeout=std::chrono::seconds(30);
        options.security.max_frames_per_second=100000;
        dc24h::network::TcpServer network(logger,options);
        dc24h::core::Server core(logger,&network);
        std::jthread termination([&](std::stop_token stop) {
            if (signals.wait(stop)) { core.request_stop(); }
        });
        core.run();
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
