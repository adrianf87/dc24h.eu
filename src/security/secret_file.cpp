#include "dc24h/security/secret_file.hpp"

#include <openssl/crypto.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <stdexcept>
#include <string_view>

namespace dc24h::security {
std::string read_private_secret(const std::string& path) {
    const auto fail = [] { throw std::runtime_error("Cannot read private credential file"); };
    if (path.empty() || path.find('\0') != std::string::npos) { fail(); }
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) { fail(); }
    struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
    struct stat state{};
    if (::fstat(fd, &state) != 0 || !S_ISREG(state.st_mode) || state.st_uid != ::geteuid() ||
        (state.st_mode & 0077) != 0 || state.st_nlink != 1 || state.st_size < 1 || state.st_size > 4096) {
        fail();
    }
    std::array<char, 4097> buffer{};
    struct Erase { std::array<char, 4097>& value; ~Erase() { OPENSSL_cleanse(value.data(), value.size()); } } erase{buffer};
    std::size_t size = 0;
    for (;;) {
        const auto count = ::read(fd, buffer.data() + size, buffer.size() - size);
        if (count < 0 && errno == EINTR) { continue; }
        if (count < 0) { fail(); }
        if (count == 0) { break; }
        size += static_cast<std::size_t>(count);
        if (size > 4096) { fail(); }
    }
    if (size > 0 && buffer[size - 1] == '\n') { --size; if (size > 0 && buffer[size - 1] == '\r') { --size; } }
    const std::string_view secret(buffer.data(), size);
    if (secret.empty() || std::any_of(secret.begin(), secret.end(), [](unsigned char c) { return c < 32 || c == 127; })) { fail(); }
    return std::string(secret);
}
} // namespace dc24h::security
