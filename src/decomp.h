#ifndef DECOMP_H
#define DECOMP_H

// `force_active on` makes MWCC emit the out-of-line copy retail has, even
// though the function is inline. Other compilers ignore the pragma, so the
// bare `inline` is emitted only at -O0; from -O1 the definition is inlined
// away and discarded, and callers in OTHER TUs fail to link. `used` restores
// the emission the pragma provides, and keeps it weak so TUs still merge.
// (This is why the native rb3-dta target linked only in an unoptimized build.)
#ifdef __MWERKS__
#define FORCE_LOCAL_INLINE _Pragma("push") _Pragma("force_active on") inline
#define END_FORCE_LOCAL_INLINE _Pragma("pop")
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_LOCAL_INLINE __attribute__((used)) inline
#define END_FORCE_LOCAL_INLINE
#else
#define FORCE_LOCAL_INLINE inline
#define END_FORCE_LOCAL_INLINE
#endif

#ifdef VERSION_SZBE69_B8
#define UNPOOL_DATA _Pragma("push") _Pragma("pool_data off")
#define END_UNPOOL_DATA _Pragma("pop")
#else
#define UNPOOL_DATA
#define END_UNPOOL_DATA
#endif

/**
 * https://github.com/kiwi515/ogws/blob/master/include/decomp.h
 * Codewarrior tricks for matching decomp
 * (Macros generate prototypes to satisfy -requireprotos)
 */

#define __CONCAT(x, y) x##y
#define CONCAT(x, y) __CONCAT(x, y)

// Compile without matching hacks.
#if defined(NON_MATCHING) || !defined(__MWERKS__)
#define DECOMP_FORCEACTIVE(module, ...)
#define DECOMP_FORCELITERAL(module, ...)
#define DECOMP_FORCEFUNC(module, decl, ...)
#define DECOMP_FORCEFUNC_TEMPL(module, cls, func, ...)
#define DECOMP_FORCEDTOR(module, cls)
#define DECOMP_FORCEBLOCK(module, ...)
// Compile with matching hacks.
// (This version of CW does not support pragmas inside macros.)
#else
// Force reference specific data
#define DECOMP_FORCEACTIVE(module, ...)                                                  \
    void fake_function(...);                                                             \
    void CONCAT(FORCEACTIVE##module, __LINE__)(void);                                    \
    void CONCAT(FORCEACTIVE##module, __LINE__)(void) { fake_function(__VA_ARGS__); }

// Force literal ordering, such as floats in sdata2
#define DECOMP_FORCELITERAL(module, ...)                                                 \
    void CONCAT(FORCELITERAL##module, __LINE__)(void);                                   \
    void CONCAT(FORCELITERAL##module, __LINE__)(void) { (__VA_ARGS__); }

// Force referenced functions
#define DECOMP_FORCEFUNC(module, cls, func)                                              \
    void CONCAT(FORCEFUNC##module, __LINE__)(cls * dummy);                               \
    void CONCAT(FORCEFUNC##module, __LINE__)(cls * dummy) { dummy->func; }

// Force referenced functions using templates
#define DECOMP_FORCEFUNC_TEMPL(module, cls, func, ...)                                   \
    void CONCAT(FORCEFUNC##module, __LINE__)(cls<__VA_ARGS__> * dummy);                  \
    void CONCAT(FORCEFUNC##module, __LINE__)(cls<__VA_ARGS__> * dummy) { dummy->func; }

// Force referenced destructor
#define DECOMP_FORCEDTOR(module, cls) DECOMP_FORCEFUNC(module, cls, ~cls())

// For more complex forcing requirements
// Example usage: DECOMP_FORCEBLOCK(Module, (Class* dummy, int arg),
//     dummy->Method(arg);
// )
#define DECOMP_FORCEBLOCK(module, params, ...)                                           \
    void CONCAT(FORCEBLOCK##module, __LINE__) params;                                    \
    void CONCAT(FORCEBLOCK##module, __LINE__) params { __VA_ARGS__ }
#endif

/*
 * ASM macros, to prevent IDEs from complaining
 */
#if defined(__MWERKS__) && !defined(DECOMP_IDE_FLAG)
#define ASM_DECL asm
#define ASM_BLOCK asm
#else
#define ASM_DECL
#define ASM_BLOCK(...)
#endif

#endif
