#pragma once

#include <csignal>
#include <optional>
#include <stop_token>

namespace dc24h::core {

// Process-level setup: construct on the main thread BEFORE creating any threads.
// SIGINT/SIGTERM remain blocked until process exit, including during final cleanup.
class TerminationSignals final {
public:
    TerminationSignals();
    TerminationSignals(const TerminationSignals&) = delete;
    TerminationSignals& operator=(const TerminationSignals&) = delete;
    // Called by exactly one waiter thread. No asynchronous signal handlers are used.
    [[nodiscard]] std::optional<int> wait(std::stop_token stop) const;

private:
    sigset_t signals_{};
};

} // namespace dc24h::core
