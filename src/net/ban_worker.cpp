#include "ban_worker.hpp"

namespace dc24h::net {
namespace {

constexpr std::size_t max_pending_checks = 32;
constexpr auto check_lifetime = std::chrono::seconds(3);
constexpr auto retry_delay = std::chrono::milliseconds(250);

} // namespace

BanWorker::BanWorker(db::Config config)
    : thread_([this, config = std::move(config)] { run(config); }) {}

BanWorker::~BanWorker() { stop(); }

bool BanWorker::submit(std::string cid, std::string ip, Callback complete) {
    {
        const std::lock_guard lock(mutex_);
        if (stopping_ || requests_.size() >= max_pending_checks) {
            return false;
        }
        requests_.push_back(
            {std::move(cid), std::move(ip), std::move(complete), Clock::now() + check_lifetime});
    }
    ready_.notify_one();
    return true;
}

void BanWorker::stop() {
    {
        const std::lock_guard lock(mutex_);
        stopping_ = true;
        requests_.clear();
    }
    ready_.notify_one();
}

void BanWorker::join() { thread_.join(); }

std::optional<BanWorker::Request> BanWorker::take() {
    std::unique_lock lock(mutex_);
    ready_.wait(lock, [this] { return stopping_ || !requests_.empty(); });
    if (stopping_) {
        return std::nullopt;
    }
    auto request = std::move(requests_.front());
    requests_.pop_front();
    return request;
}

void BanWorker::run(const db::Config &config) {
    std::optional<db::Store> store;
    auto retry_at = Clock::time_point::min();
    while (auto request = take()) {
        BanDecision result;
        const auto now = Clock::now();
        if (now < request->deadline && now >= retry_at) {
            try {
                if (!store) {
                    store.emplace(config);
                    store->require_current_schema();
                }
                auto ban = store->active_ban(request->cid, request->ip);
                if (Clock::now() < request->deadline) {
                    result = {true, std::move(ban)};
                }
            } catch (const db::Error &) {
                store.reset();
                retry_at = Clock::now() + retry_delay;
            } catch (const std::invalid_argument &) {
                // An unusable socket identity cannot pass admission.
            }
        }
        request->complete(std::move(result));
    }
}

} // namespace dc24h::net
