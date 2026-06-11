#pragma once

// ============================================================================
// Xenon Application Binary Interface (ABI) Macrology
// ============================================================================

// Enforce uniform C-Linkage to block C++ symbol name mangling.
// Your Code Generator can seamlessly declare functions using this macro.
#ifdef __cplusplus
    #define XENON_ABI extern "C"
#else
    #define XENON_ABI
#endif

// Define custom platform attributes for performance optimizations.
// Forces the C++ compiler to aggressively inline critical backend glue code.
#if defined(_MSC_VER)
    #define XENON_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define XENON_ALWAYS_INLINE inline __attribute__((always_inline))
#else
    #define XENON_ALWAYS_INLINE inline
#endif

// Hints to the backend optimizer that a function will never return an error
// or throw exceptions, reducing the overhead of stack unwinding tables.
#if defined(__GNUC__) || defined(__clang__)
    #define XENON_NOTHROW __attribute__((nothrow))
#else
    #define XENON_NOTHROW
#endif