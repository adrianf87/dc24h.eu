#pragma once

#include <string>

namespace dc24h::security {
// Effective-UID-owned regular file, no group/other access, no final symlink,
// one hard link, 1..4096 bytes. Strips one final LF/CRLF; rejects controls/NUL.
// Diagnostic text never contains the path or file contents.
[[nodiscard]] std::string read_private_secret(const std::string& path);
} // namespace dc24h::security
