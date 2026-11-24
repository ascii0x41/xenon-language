
#pragma once

#include <cstdint>

namespace xenon::lexer {

    using uchar = std::uint8_t;

    // UTF-8 byte classification

    // UTF-8 continuation bytes have the form 10xxxxxx.
    inline constexpr bool is_utf8_continuation(uchar c) noexcept {
        return (c & 0xC0) == 0x80;
    }

    // Character classification

    inline constexpr bool is_whitespace(uchar c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    inline constexpr bool is_digit(uchar c) noexcept {
        return c >= '0' && c <= '9';
    }

    inline constexpr bool is_hex_digit(uchar c) noexcept {
        return is_digit(c)
            || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F');
    }

    inline constexpr bool is_bin_digit(uchar c) noexcept {
        return c == '0' || c == '1';
    }

    inline constexpr bool is_oct_digit(uchar c) noexcept {
        return c >= '0' && c <= '7';
    }

    // Identifier classification

    // ASCII letters, underscore, or non-ASCII leading bytes.
    // Non-ASCII bytes are accepted permissively; UTF-8 validity
    // and Unicode identifier rules are not checked here.
    inline constexpr bool is_ident_start(uchar c) noexcept {
        return (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || c == '_'
            || c >= 0x80;
    }

    inline constexpr bool is_ident_continue(uchar c) noexcept {
        return is_ident_start(c)
            || is_digit(c)
            || is_utf8_continuation(c);
    }

} // namespace xenon::lexer
