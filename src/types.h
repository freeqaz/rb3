#ifndef RB3_TYPES_H
#define RB3_TYPES_H
#include "macros.h" /* IWYU pragma: keep */
#include <stddef.h>

typedef int BOOL;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef HX_NATIVE
// Native build: x86_64 Linux, clang LP64. `long` is 8 bytes here, so the Wii
// `signed long`/`unsigned long` defs below would make s32/u32 8 bytes (wrong).
// Use int-width types to keep the 32-bit ABI the matched fork assumes.
// Mirrors rb3-xenon commit 31a0e1d (which adopts dc3's dual-target types.h).
#include <strings.h> // strcasecmp / strncasecmp
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int uint;
typedef unsigned int u32;
typedef unsigned long long u64;
#else
// Wii build (MWCC, Gekko/Broadway, ILP32): `long` is 4 bytes — keep verbatim.
typedef signed char s8;
typedef signed short s16;
typedef signed long s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int uint;
typedef unsigned long u32;
typedef unsigned long long u64;
#endif

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

typedef float f32;
typedef double f64;
typedef volatile f32 vf32;
typedef volatile f64 vf64;

typedef unsigned char byte_t;
#ifndef HX_NATIVE
// On LP64 native, register_t is already provided by <sys/types.h> (as long);
// redefining it here conflicts. Only the Wii build needs this typedef.
typedef unsigned int register_t;
#endif

#endif
