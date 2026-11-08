#include "dc24h/core/server.hpp"
#include "dc24h/core/termination_signals.hpp"
#include "dc24h/logging/logger.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <vector>
#include <unistd.h>

namespace {
using dc24h::core::Server;
using dc24h::core::State;
using namespace std::chrono_literals;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expect_running(Server& server) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (server.state() != State::running && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    require(server.state() == State::running, "Core did not reach running state");
}

void lifecycle() {
    dc24h::logging::Logger logger;
    Server server(logger);
    require(server.state() == State::created, "Initial state must be created");
    std::jthread worker([&](std::stop_token stop) {
        std::stop_callback cleanup(stop, [&] { server.request_stop(); });
        server.run();
    });
    expect_running(server);
    server.request_stop();
    worker.join();
    require(server.state() == State::stopped, "Core must stop");
    server.request_stop();
    require(server.state() == State::stopped, "Stop must be idempotent");
    bool rejected = false;
    try { server.run(); } catch (const std::logic_error&) { rejected = true; }
    require(rejected, "Restarting a used Core must be rejected");
}

void stop_before_start() {
    dc24h::logging::Logger logger;
    Server server(logger);
    server.request_stop();
    require(server.state() == State::stopping, "Early stop must be retained");
    server.run();
    require(server.state() == State::stopped, "Early stop must not be lost");
}

void concurrent_stop() {
    for (int iteration = 0; iteration < 20; ++iteration) {
        dc24h::logging::Logger logger;
        Server server(logger);
        std::jthread worker([&](std::stop_token stop) {
            std::stop_callback cleanup(stop, [&] { server.request_stop(); });
            server.run();
        });
        expect_running(server);
        bool rejected = false;
        try { server.run(); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "Concurrent run must be rejected");
        std::vector<std::jthread> callers;
        for (int index = 0; index < 8; ++index) {
            callers.emplace_back([&] { server.request_stop(); });
        }
        callers.clear();
        worker.join();
        require(server.state() == State::stopped, "Concurrent stop failed");
    }
}

void signal_wait() {
    // Last test: process-level signal mask deliberately lives until process exit.
    const dc24h::core::TerminationSignals signals;
    std::stop_source cancelled;
    cancelled.request_stop();
    require(!signals.wait(cancelled.get_token()), "Cancelled wait must return no signal");

    std::jthread waiter([&](std::stop_token stop) {
        require(!signals.wait(stop), "Wait cancellation must not invent a signal");
    });
    std::this_thread::sleep_for(10ms);
    waiter.request_stop();
    waiter.join();

    require(::kill(::getpid(), SIGTERM) == 0, "Cannot queue SIGTERM");
    require(signals.wait({}) == SIGTERM, "Pending SIGTERM must be consumed");
}
} // namespace

int main() {
    try {
        lifecycle();
        stop_before_start();
        concurrent_stop();
        signal_wait();
        std::cout << "PASS: lifecycle, early stop, concurrent stop, signal wait\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
