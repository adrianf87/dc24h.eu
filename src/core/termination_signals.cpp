#include "dc24h/core/termination_signals.hpp"

#include <cerrno>
#include <pthread.h>
#include <system_error>
#include <time.h>

namespace dc24h::core {

TerminationSignals::TerminationSignals() {
    if (::sigemptyset(&signals_) == -1 || ::sigaddset(&signals_, SIGINT) == -1 ||
        ::sigaddset(&signals_, SIGTERM) == -1) {
        throw std::system_error(errno, std::generic_category(), "build signal set");
    }
    const int error = ::pthread_sigmask(SIG_BLOCK, &signals_, nullptr);
    if (error != 0) {
        throw std::system_error(error, std::generic_category(), "block termination signals");
    }
}

std::optional<int> TerminationSignals::wait(std::stop_token stop) const {
    // A bounded wait lets the jthread join after a programmatic stop or startup error.
    constexpr timespec timeout{0, 100'000'000};
    while (!stop.stop_requested()) {
        const int signal = ::sigtimedwait(&signals_, nullptr, &timeout);
        if (signal > 0) {
            return signal;
        }
        const int error = errno;
        if (error != EAGAIN && error != EINTR) {
            throw std::system_error(error, std::generic_category(), "wait for termination signal");
        }
    }
    return std::nullopt;
}

} // namespace dc24h::core
