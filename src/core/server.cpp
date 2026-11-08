#include "dc24h/core/server.hpp"
#include "dc24h/logging/logger.hpp"
#include "dc24h/network/tcp_server.hpp"

#include <stdexcept>

namespace dc24h::core {

Server::Server(logging::Logger& logger, network::TcpServer* network) noexcept
    : logger_(logger), network_(network) {}

void Server::run() {
    std::unique_lock lock(mutex_);
    if (run_started_) {
        throw std::logic_error("Server::run() may only be called once");
    }
    run_started_ = true;
    if (!stop_requested_) {
        state_ = State::running;
        logger_.info("Core running");
        if (network_ != nullptr) {
            lock.unlock();
            try {
                network_->run();
            } catch (...) {
                // Transport RAII has already closed partial startup and client resources.
                lock.lock();
                state_ = State::stopped;
                logger_.error("Core stopped after Network failure");
                throw;
            }
            lock.lock();
        } else {
            wakeup_.wait(lock, [this] { return stop_requested_; });
        }
    }

    state_ = State::stopping;
    logger_.info("Core stopping");
    // The Network run loop has released its sockets before reaching this point.
    state_ = State::stopped;
    logger_.info("Core stopped");
}

void Server::request_stop() {
    {
        const std::lock_guard lock(mutex_);
        stop_requested_ = true;
        if (state_ != State::stopped) {
            state_ = State::stopping;
        }
    }
    if (network_ != nullptr) { network_->request_stop(); }
    wakeup_.notify_all();
}

State Server::state() const {
    const std::lock_guard lock(mutex_);
    return state_;
}

} // namespace dc24h::core
