#pragma once

#include "dc24h/db/store.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace dc24h::net {

struct BanDecision {
    bool available = false;
    std::optional<db::ActiveBan> ban;
};

class BanWorker {
  public:
    explicit BanWorker(db::Config config);
    ~BanWorker();

    using Callback = std::function<void(BanDecision)>;
    [[nodiscard]] bool submit(std::string cid, std::string ip, Callback complete);
    void stop();
    void join();

  private:
    using Clock = std::chrono::steady_clock;

    struct Request {
        std::string cid;
        std::string ip;
        Callback complete;
        Clock::time_point deadline;
    };

    [[nodiscard]] std::optional<Request> take();
    void run(const db::Config &config);

    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Request> requests_;
    bool stopping_ = false;
    std::jthread thread_;
};

} // namespace dc24h::net
