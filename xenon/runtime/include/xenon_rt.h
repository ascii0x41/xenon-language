// xenon_rt.h
#ifndef XENON_RT_H
#define XENON_RT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// Xenon Core Runtime Definitions & Primitive Types
// ============================================================================

// Signed Integers
typedef int8_t   XENON_I8;
typedef int16_t  XENON_I16;
typedef int32_t  XENON_I32;
typedef int64_t  XENON_I64;

// Unsigned Integers
typedef uint8_t  XENON_U8;
typedef uint16_t XENON_U16;
typedef uint32_t XENON_U32;
typedef uint64_t XENON_U64;

// Floating-point
typedef float  XENON_F32;
typedef double XENON_F64;

// Native complex type
typedef struct {
    XENON_F64 real;
    XENON_F64 imag;
} XENON_CPLX128;

// Platform-dependent architecture size type
typedef size_t XENON_SIZE;

// Booleans & Characters
typedef bool XENON_BOOL;
typedef uint32_t XENON_CHAR; // UTF-32 for standalone characters

// Built-in Immutable String Types
typedef struct {
    const XENON_U8* bytes;  // Const pointer to Read-Only UTF-8 bytes
    XENON_SIZE length;      // Immutable byte length tracking
} XENON_STRING;

// ============================================================================
// ABI Macros (for C)
// ============================================================================

#ifdef _MSC_VER
    #define XENON_ABI __declspec(dllexport)
    #define XENON_ALWAYS_INLINE __forceinline
    #define XENON_NOTHROW __declspec(nothrow)
#else
    #define XENON_ABI __attribute__((visibility("default")))
    #define XENON_ALWAYS_INLINE inline __attribute__((always_inline))
    #define XENON_NOTHROW __attribute__((nothrow))
#endif

// ============================================================================
// Complex Number Operations
// ============================================================================

XENON_ABI XENON_CPLX128 xenon_init_complex(XENON_F64 real, XENON_F64 imag);
XENON_ABI XENON_CPLX128 xenon_cplx128_add(XENON_CPLX128 a, XENON_CPLX128 b);
XENON_ABI XENON_CPLX128 xenon_cplx128_sub(XENON_CPLX128 a, XENON_CPLX128 b);
XENON_ABI XENON_CPLX128 xenon_cplx128_mul(XENON_CPLX128 a, XENON_CPLX128 b);
XENON_ABI XENON_CPLX128 xenon_cplx128_div(XENON_CPLX128 a, XENON_CPLX128 b);
XENON_ABI void xenon_cplx128_iadd(XENON_CPLX128* a, XENON_CPLX128 b);
XENON_ABI void xenon_cplx128_isub(XENON_CPLX128* a, XENON_CPLX128 b);
XENON_ABI void xenon_cplx128_imul(XENON_CPLX128* a, XENON_CPLX128 b);
XENON_ABI void xenon_cplx128_idiv(XENON_CPLX128* a, XENON_CPLX128 b);
XENON_ABI bool xenon_cplx128_eq(XENON_CPLX128 a, XENON_CPLX128 b);
XENON_ABI bool xenon_cplx128_neq(XENON_CPLX128 a, XENON_CPLX128 b);
XENON_ABI XENON_F64 xenon_cplx128_abs(XENON_CPLX128 c);
XENON_ABI XENON_CPLX128 xenon_cplx128_conj(XENON_CPLX128 c);
XENON_ABI XENON_CPLX128 xenon_cplx128_polar(XENON_F64 magnitude, XENON_F64 angle);

// ============================================================================
// String Operations
// ============================================================================

XENON_ABI XENON_STRING xenon_init_string(const XENON_U8* bytes, XENON_SIZE length);
XENON_ABI XENON_STRING xenon_string_from_cstr(const char* str); // String creation from C string (convenience for codegen)
XENON_ABI XENON_STRING xenon_string_add(XENON_STRING a, XENON_STRING b);
XENON_ABI XENON_STRING xenon_string_mul(XENON_STRING a, XENON_SIZE times);
XENON_ABI bool xenon_string_eq(XENON_STRING a, XENON_STRING b);
XENON_ABI bool xenon_string_neq(XENON_STRING a, XENON_STRING b);
XENON_ABI void xenon_string_drop(XENON_STRING* str);

// ============================================================================
// Runtime Utilities
// ============================================================================

XENON_ABI void xenon_move(void** dest, void** src);
XENON_ABI XENON_NOTHROW void xenon_panic(XENON_STRING message);
XENON_ABI XENON_NOTHROW void xenon_assert(XENON_BOOL condition, XENON_STRING message);
XENON_ABI XENON_NOTHROW void xenon_exit(XENON_I32 code);
XENON_ABI XENON_NOTHROW void xenon_println(XENON_STRING str);

// ============================================================================
// Program Entry Point
// ============================================================================

// This is the actual entry point. It initializes the runtime, then calls
// into LLVM IR generated code. The user's program should define:
//
//   void xenon_program_main(void)
//
// in their LLVM IR (or C if they're being weird about it).
XENON_ABI int main(int, char**);

// The user program implements this. Called from _start() after runtime init.
void xenon_program_main(void);

// Register a destructor to run at exit (for static vars, etc.)
XENON_ABI void xenon_register_atexit(void (*func)(void));

// ============================================================================
// Additional Runtime Functions You'll Want
// ============================================================================

// Memory allocation/deallocation exposed to LLVM IR
XENON_ABI void* xenon_alloc(XENON_SIZE size);
XENON_ABI void xenon_free(void* ptr);
XENON_ABI void* xenon_realloc(void* ptr, XENON_SIZE new_size);

// Type operations for array/string internals
XENON_ABI void* xenon_array_alloc(XENON_SIZE element_size, XENON_SIZE count);
XENON_ABI void xenon_array_free(void* data);

#endif // XENON_RT_H