#include "../../include/xenon_rt.h"

inline XENON_STRING operator+(const XENON_STRING& a, const XENON_STRING& b) {
    if (a.length == 0) return b;
    if (b.length == 0) return a;

    XENON_SIZE new_length = a.length + b.length;
    XENON_U8* new_buffer = new XENON_U8[new_length];

    std::memcpy(new_buffer, a.bytes, a.length);
    std::memcpy(new_buffer + a.length, b.bytes, b.length);

    return XENON_STRING{ new_buffer, new_length };
}

XENON_SIZE size(const XENON_STRING& str) {
    return str.length;
}

XENON_CHAR index(const XENON_STRING& str, XENON_SIZE index) {
    if (index >= str.length) {
        // Out-of-bounds access; return null character
        return U'\0';
    }
    return static_cast<XENON_CHAR>(str.bytes[index]);
}

