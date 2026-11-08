#include "dc24h/core/server.hpp"
#include "dc24h/logging/logger.hpp"

#include <stdexcept>

namespace dc24h::core {

Server::Server(logging::Logger& logger) noexcept : logger_(logger) {}

void Server::run() {
    std::unique_lock lock(mutex_);
    if (run_started_) {
        throw std::logic_error("Server::run() may only be called once");
    }
    run_started_ = true;
    if (!stop_requested_) {
        state_ = State::running;
        logger_.info("Core running; waiting for stop request");
        wakeup_.wait(lock, [this] { return stop_requested_; });
    }

    state_ = State::stopping;
    logger_.info("Core stopping");
    // Future modules must release their resources here, in reverse startup order.
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
    wakeup_.notify_all();
}

State Server::state() const {
    const std::lock_guard lock(mutex_);
    return state_;
}

} // namespace dc24h::core
