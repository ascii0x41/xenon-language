// xenon_rt.c
#include "xenon_rt.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// ============================================================================
// String Operations
// ============================================================================

XENON_ABI XENON_STR xenon_init_string(const XENON_U8* bytes, XENON_SIZE length) {
    XENON_STR result;
    XENON_U8* new_bytes = (XENON_U8*)malloc(length > 0 ? length : 1);
    if (length > 0 && bytes) {
        memcpy(new_bytes, bytes, length);
    }
    result.bytes = new_bytes;
    result.length = length;
    return result;
}

XENON_ABI XENON_STR xenon_string_from_cstr(const char* str) {
    return xenon_init_string((const XENON_U8*)str, strlen(str));
}


XENON_ABI XENON_STR xenon_string_add(XENON_STR a, XENON_STR b) {
    XENON_STR result;
    result.length = a.length + b.length;
    XENON_U8* new_bytes = (XENON_U8*)malloc(result.length > 0 ? result.length : 1);
    
    if (a.length > 0) memcpy(new_bytes, a.bytes, a.length);
    if (b.length > 0) memcpy(new_bytes + a.length, b.bytes, b.length);
    
    result.bytes = new_bytes;
    return result;
}

XENON_ABI XENON_STR xenon_string_mul(XENON_STR a, XENON_SIZE times) {
    XENON_STR result;
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

XENON_ABI XENON_BOOL xenon_string_eq(XENON_STR a, XENON_STR b) {
    if (a.length != b.length) return false;
    return memcmp(a.bytes, b.bytes, a.length) == 0;
}

XENON_ABI XENON_BOOL xenon_string_neq(XENON_STR a, XENON_STR b) {
    return !xenon_string_eq(a, b);
}

XENON_ABI void xenon_string_drop(XENON_STR* str) {
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

XENON_ABI XENON_NOTHROW void xenon_panic(XENON_STR message) {
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

XENON_ABI XENON_NOTHROW void xenon_assert(XENON_BOOL condition, XENON_STR message) {
    if (!condition) {
        xenon_panic(message);
    }
}

XENON_ABI XENON_NOTHROW void xenon_exit(XENON_I32 code) {
    exit(code);
}

XENON_ABI XENON_NOTHROW void xenon_println(XENON_STR str) {
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
XENON_ABI int main(void) {
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

XENON_ABI void* xenon_array_alloc(XENON_SIZE element_size, XENON_SIZE count) {
    if (count == 0) return NULL;

    if (element_size > SIZE_MAX / count) {
        return NULL;
    }

    return malloc(element_size * count);
}

XENON_ABI void xenon_array_free(void* ptr) {
    free(ptr);
}