#pragma once

#include <cstddef>
#include <string>

// OpenSSL handles never escape the ADCS transport module.
struct ssl_ctx_st;
struct ssl_st;

namespace dc24h::adcs {

enum class IoStatus { complete, want_read, want_write, closed, error };
struct IoResult {
    IoStatus status;
    std::size_t bytes{0};
};

class Context final {
public:
    Context(const std::string& certificate_chain, const std::string& private_key);
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

private:
    friend class Connection;
    ssl_ctx_st* context_{nullptr};
};

class Connection final {
public:
    // The fd and Context must outlive this Connection.
    Connection(const Context& context, int fd);
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Each call makes one nonblocking OpenSSL operation. The event loop must
    // resume it only when the requested readiness (READ/WRITE) is available.
    [[nodiscard]] IoResult handshake() noexcept;
    [[nodiscard]] IoResult receive(char* buffer, std::size_t capacity) noexcept;

private:
    ssl_st* ssl_{nullptr};
};

} // namespace dc24h::adcs
