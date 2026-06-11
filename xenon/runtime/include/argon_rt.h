#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

// ============================================================================
// Xenon Core Runtime Definitions & Primitive Types
// ============================================================================

// Signed Integers
using XENON_I8   = int8_t;
using XENON_I16  = int16_t;
using XENON_I32  = int32_t;
using XENON_I64  = int64_t;

// Unsigned Integers
using XENON_U8   = uint8_t;
using XENON_U16  = uint16_t;
using XENON_U32  = uint32_t;
using XENON_U64  = uint64_t;

// Floating-point
using XENON_F32  = float;
using XENON_F64  = double;

// Native complex type
struct XENON_COMPLEX {
    XENON_F64 real;
    XENON_F64 imag;
};

// Booleans & Characters
using XENON_BOOL = bool;
using XENON_CHAR = char32_t; // UTF-32 for standalone characters

// Platform-dependent xechitecture size type (Unsigned Pointer Width)
// Maps directly to Xenon's 'Size' type
using XENON_SIZE = size_t;

// Built-in Immutable String Types
struct XENON_STRING {
    const XENON_U8* const bytes; // Const pointer to Read-Only UTF-8 bytes
    const XENON_SIZE length;     // Immutable byte length tracking
};

XENON_STRING operator+ (const XENON_STRING& a, const XENON_STRING& b) {
    if (a.length == 0) return b;
    if (b.length == 0) return a;

    XENON_SIZE new_length = a.length + b.length;
    XENON_U8* new_buffer = new XENON_U8[new_length];

    std::memcpy(new_buffer, a.bytes, a.length);
    std::memcpy(new_buffer + a.length, b.bytes, b.length);

    return XENON_STRING{ new_buffer, new_length };
}

