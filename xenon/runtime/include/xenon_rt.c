// xenon_rt.c
#include "xenon_rt.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// ============================================================================
// Complex Number Operations
// ============================================================================

XENON_ABI XENON_CPLX128 xenon_init_complex(XENON_F64 real, XENON_F64 imag) {
    XENON_CPLX128 result = { real, imag };
    return result;
}

XENON_ABI XENON_CPLX128 xenon_cplx128_add(XENON_CPLX128 a, XENON_CPLX128 b) {
    XENON_CPLX128 result = { a.real + b.real, a.imag + b.imag };
    return result;
}

XENON_ABI XENON_CPLX128 xenon_cplx128_sub(XENON_CPLX128 a, XENON_CPLX128 b) {
    XENON_CPLX128 result = { a.real - b.real, a.imag - b.imag };
    return result;
}

XENON_ABI XENON_CPLX128 xenon_cplx128_mul(XENON_CPLX128 a, XENON_CPLX128 b) {
    XENON_CPLX128 result;
    result.real = a.real * b.real - a.imag * b.imag;
    result.imag = a.real * b.imag + a.imag * b.real;
    return result;
}

XENON_ABI XENON_CPLX128 xenon_cplx128_div(XENON_CPLX128 a, XENON_CPLX128 b) {
    XENON_F64 denominator = b.real * b.real + b.imag * b.imag;
    XENON_CPLX128 result;
    result.real = (a.real * b.real + a.imag * b.imag) / denominator;
    result.imag = (a.imag * b.real - a.real * b.imag) / denominator;
    return result;
}

XENON_ABI void xenon_cplx128_iadd(XENON_CPLX128* a, XENON_CPLX128 b) {
    a->real += b.real;
    a->imag += b.imag;
}

XENON_ABI void xenon_cplx128_isub(XENON_CPLX128* a, XENON_CPLX128 b) {
    a->real -= b.real;
    a->imag -= b.imag;
}

XENON_ABI void xenon_cplx128_imul(XENON_CPLX128* a, XENON_CPLX128 b) {
    XENON_F64 old_real = a->real;
    a->real = a->real * b.real - a->imag * b.imag;
    a->imag = old_real * b.imag + a->imag * b.real;
}

XENON_ABI void xenon_cplx128_idiv(XENON_CPLX128* a, XENON_CPLX128 b) {
    XENON_F64 denominator = b.real * b.real + b.imag * b.imag;
    XENON_F64 old_real = a->real;
    a->real = (a->real * b.real + a->imag * b.imag) / denominator;
    a->imag = (a->imag * b.real - old_real * b.imag) / denominator;
}

XENON_ABI bool xenon_cplx128_eq(XENON_CPLX128 a, XENON_CPLX128 b) {
    return a.real == b.real && a.imag == b.imag;
}

XENON_ABI bool xenon_cplx128_neq(XENON_CPLX128 a, XENON_CPLX128 b) {
    return !(a.real == b.real && a.imag == b.imag);
}

XENON_ABI XENON_F64 xenon_cplx128_abs(XENON_CPLX128 c) {
    return sqrt(c.real * c.real + c.imag * c.imag);
}

XENON_ABI XENON_CPLX128 xenon_cplx128_conj(XENON_CPLX128 c) {
    XENON_CPLX128 result = { c.real, -c.imag };
    return result;
}

XENON_ABI XENON_CPLX128 xenon_cplx128_polar(XENON_F64 magnitude, XENON_F64 angle) {
    XENON_CPLX128 result = { magnitude * cos(angle), magnitude * sin(angle) };
    return result;
}

// ============================================================================
// String Operations
// ============================================================================

XENON_ABI XENON_STRING xenon_init_string(const XENON_U8* bytes, XENON_SIZE length) {
    XENON_STRING result;
    XENON_U8* new_bytes = (XENON_U8*)malloc(length > 0 ? length : 1);
    if (length > 0 && bytes) {
        memcpy(new_bytes, bytes, length);
    }
    result.bytes = new_bytes;
    result.length = length;
    return result;
}

XENON_ABI XENON_STRING xenon_string_from_cstr(const char* str) {
    return xenon_init_string((const XENON_U8*)str, strlen(str));
}


XENON_ABI XENON_STRING xenon_string_add(XENON_STRING a, XENON_STRING b) {
    XENON_STRING result;
    result.length = a.length + b.length;
    XENON_U8* new_bytes = (XENON_U8*)malloc(result.length > 0 ? result.length : 1);
    
    if (a.length > 0) memcpy(new_bytes, a.bytes, a.length);
    if (b.length > 0) memcpy(new_bytes + a.length, b.bytes, b.length);
    
    result.bytes = new_bytes;
    return result;
}

XENON_ABI XENON_STRING xenon_string_mul(XENON_STRING a, XENON_SIZE times) {
    XENON_STRING result;
    result.length = a.length * times;
    
    // Handle zero-length edge case
    XENON_U8* new_bytes = (XENON_U8*)malloc(result.length > 0 ? result.length : 1);
    
    for (XENON_SIZE i = 0; i < times; ++i) {
        if (a.length > 0) {
            memcpy(new_bytes + i * a.length, a.bytes, a.length);
        }
    }
    
    result.bytes = new_bytes;
    return result;
}

XENON_ABI bool xenon_string_eq(XENON_STRING a, XENON_STRING b) {
    if (a.length != b.length) return false;
    return memcmp(a.bytes, b.bytes, a.length) == 0;
}

XENON_ABI bool xenon_string_neq(XENON_STRING a, XENON_STRING b) {
    return !xenon_string_eq(a, b);
}

XENON_ABI void xenon_string_drop(XENON_STRING* str) {
    if (str && str->bytes) {
        free((void*)str->bytes);
        str->bytes = NULL;
        str->length = 0;
    }
}

// ============================================================================
// Runtime Utilities
// ============================================================================

XENON_ABI void xenon_move(void** dest, void** src) {
    *dest = *src;
    *src = NULL;
}

XENON_ABI XENON_NOTHROW void xenon_panic(XENON_STRING message) {
    fprintf(stderr, "\n--- XENON RUNTIME PANIC ---\n");
    fprintf(stderr, "The application encountered an unrecoverable error and aborted.\n\n");
    fprintf(stderr, "Reason: ");
    
    if (message.bytes && message.length > 0) {
        // Write the message safely (may contain embedded nulls)
        fwrite(message.bytes, 1, message.length, stderr);
    } else {
        fprintf(stderr, "[No panic message provided]");
    }
    
    fprintf(stderr, "\n---------------------------\n");
    exit(101);
}

XENON_ABI XENON_NOTHROW void xenon_assert(XENON_BOOL condition, XENON_STRING message) {
    if (!condition) {
        xenon_panic(message);
    }
}

XENON_ABI XENON_NOTHROW void xenon_exit(XENON_I32 code) {
    exit(code);
}

XENON_ABI XENON_NOTHROW void xenon_println(XENON_STRING str) {
    if (str.bytes && str.length > 0) {
        fwrite(str.bytes, 1, str.length, stdout);
    } else {
        printf("[Empty String]");
    }
    printf("\n");
}

// ============================================================================
// Program Entry Point
// ============================================================================

// Array of atexit callbacks (simplified - just a fixed-size array)
#define MAX_ATEXIT_CALLBACKS 64
static void (*g_atexit_callbacks[MAX_ATEXIT_CALLBACKS])(void) = {0};
static size_t g_atexit_count = 0;

XENON_ABI void xenon_register_atexit(void (*func)(void)) {
    if (g_atexit_count < MAX_ATEXIT_CALLBACKS) {
        g_atexit_callbacks[g_atexit_count++] = func;
    }
}

// The actual entry point
XENON_ABI int main(int argc, char** argv) {
    xenon_program_main();
    return 0;
}

// ============================================================================
// Memory Operations (for LLVM IR to call)
// ============================================================================

XENON_ABI void* xenon_alloc(XENON_SIZE size) {
    return malloc(size);
}

XENON_ABI void xenon_free(void* ptr) {
    free(ptr);
}

XENON_ABI void* xenon_realloc(void* ptr, XENON_SIZE new_size) {
    return realloc(ptr, new_size);
}

XENON_ABI void* xenon_array_alloc(XENON_SIZE element_size, XENON_SIZE count) {
    // Allocate extra 2 words for length & capacity (like a slice header)
    // Layout: [length:8 bytes][capacity:8 bytes][elements...]
    XENON_SIZE header_size = 2 * sizeof(XENON_SIZE);
    XENON_U8* data = (XENON_U8*)malloc(header_size + element_size * count);
    if (!data) return NULL;
    
    ((XENON_SIZE*)data)[0] = count;  // length
    ((XENON_SIZE*)data)[1] = count;  // capacity
    
    return data + header_size;
}

XENON_ABI void xenon_array_free(void* data) {
    if (data) {
        XENON_U8* header = (XENON_U8*)data - 2 * sizeof(XENON_SIZE);
        free(header);
    }
}
