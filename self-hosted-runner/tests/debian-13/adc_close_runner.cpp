#include "dc24h/network/tcp_server.hpp"
#include "dc24h/core/server.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <signal.h>
#include <iostream>
#include <thread>

// Executable-local fault fixture. Production still caps kept sockets.
// This runner forces a tiny send buffer after that cap and keeps the TCP user
// timeout longer than the one-second drain deadline, so stalled output is
// closed by the application deadline rather than by the kernel.
extern "C" int __real_accept4(int, sockaddr*, socklen_t*, int);
extern "C" int __real_setsockopt(int, int, int, const void*, socklen_t);
extern "C" int __wrap_setsockopt(int fd, int level, int name, const void* value, socklen_t length) {
    if (level == SOL_SOCKET && name == SO_SNDBUF) {
        const int capacity = 1024;
        return __real_setsockopt(fd, level, name, &capacity, sizeof(capacity));
    }
    if (level == IPPROTO_TCP && name == TCP_USER_TIMEOUT) {
        const unsigned int timeout = 120000;
        return __real_setsockopt(fd, level, name, &timeout, sizeof(timeout));
    }
    return __real_setsockopt(fd, level, name, value, length);
}
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
