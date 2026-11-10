#include "dc24h/text/public_text.hpp"

#include <utf8proc.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <new>

namespace dc24h::text {
bool valid_public_text(std::string_view value, bool nickname) {
    if (value.size()>max_public_text_bytes) { return false; }
    if (value.empty()) { return true; }
    utf8proc_uint8_t* raw=nullptr;
    const auto length=utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(value.data()),
        static_cast<utf8proc_ssize_t>(value.size()),&raw,
        static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE));
    const std::unique_ptr<utf8proc_uint8_t,decltype(&std::free)> normalized(raw,&std::free);
    if (length==UTF8PROC_ERROR_NOMEM) { throw std::bad_alloc(); }
    if (length<0 || static_cast<std::size_t>(length)!=value.size() ||
        !std::equal(value.begin(),value.end(),reinterpret_cast<const char*>(raw))) { return false; }
    std::size_t offset=0;
    while (offset<value.size()) {
        utf8proc_int32_t codepoint=0;
        const auto count=utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(value.data()+offset),
            static_cast<utf8proc_ssize_t>(value.size()-offset),&codepoint);
        if (count<=0 || codepoint<(nickname ? 33 : 32)) { return false; }
        const auto category=utf8proc_category(codepoint);
        if (category==UTF8PROC_CATEGORY_CC || category==UTF8PROC_CATEGORY_CF ||
            category==UTF8PROC_CATEGORY_ZL || category==UTF8PROC_CATEGORY_ZP) { return false; }
        offset+=static_cast<std::size_t>(count);
    }
    return true;
}
bool valid_chat_text(std::string_view value) {
    if (value.empty() || value.size()>max_chat_text_bytes) { return false; }
    for (const unsigned char c:value) {
        if ((c<32 && c!='\n') || c==127) { return false; }
    }
    utf8proc_uint8_t* raw=nullptr;
    const auto length=utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(value.data()),
        static_cast<utf8proc_ssize_t>(value.size()),&raw,
        static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE));
    const std::unique_ptr<utf8proc_uint8_t,decltype(&std::free)> normalized(raw,&std::free);
    if (length==UTF8PROC_ERROR_NOMEM) { throw std::bad_alloc(); }
    return length>=0 && static_cast<std::size_t>(length)==value.size() &&
        std::equal(value.begin(),value.end(),reinterpret_cast<const char*>(raw));
}
} // namespace dc24h::text
