#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

void require(bool value, const char* reason) {
    if (!value) { throw std::runtime_error(reason); }
}

void check_heap() {
    auto* values = new int[8];
    for (int index = 0; index < 8; ++index) { values[index] = index; }
    int sum = 0;
    for (int index = 0; index < 8; ++index) { sum += values[index]; }
    delete[] values;
    require(sum == 28, "Heap round trip drifted");
}

void check_threads() {
    std::atomic<int> shared{0};
    int guarded = 0;
    std::mutex mutex;
    const auto worker = [&] {
        for (int index = 0; index < 1000; ++index) {
            shared.fetch_add(1, std::memory_order_relaxed);
            const std::lock_guard<std::mutex> lock(mutex);
            ++guarded;
        }
    };
    std::thread first(worker);
    std::thread second(worker);
    first.join();
    second.join();
    require(shared.load(std::memory_order_relaxed) == 2000, "Atomic counter drifted");
    require(guarded == 2000, "Mutex counter drifted");
}

// The arguments are hidden from the optimizer so UndefinedBehaviorSanitizer
// keeps the signed multiply check in the instrumented build.
[[gnu::noinline]] int bounded_product(int left, int right) { return left * right; }

void check_signed() {
    require(bounded_product(1000000, 2) == 2000000, "Signed product drifted");
}

const char* mode() {
#if defined(DC24H_SANITIZE_ADDRESS)
    return "address";
#elif defined(DC24H_SANITIZE_UNDEFINED)
    return "undefined";
#elif defined(DC24H_SANITIZE_THREAD)
    return "thread";
#else
    return "none";
#endif
}

void check_instrumentation() {
#if defined(DC24H_SANITIZE_ADDRESS) && !defined(__SANITIZE_ADDRESS__)
    throw std::runtime_error("AddressSanitizer was requested but this unit is not instrumented");
#elif defined(DC24H_SANITIZE_THREAD) && !defined(__SANITIZE_THREAD__)
    throw std::runtime_error("ThreadSanitizer was requested but this unit is not instrumented");
#endif
}

}

int main() {
    try {
        check_instrumentation();
        check_heap();
        check_threads();
        check_signed();
        std::cout << "Alpha 0.9.15 memory safety probe OK mode=" << mode() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
