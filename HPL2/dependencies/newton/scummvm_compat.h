#ifndef NEWTON_SCUMMVM_COMPAT_H
#define NEWTON_SCUMMVM_COMPAT_H

#include <math.h>
#include <new>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

#define ARRAYSIZE(x) ((int)(sizeof(x) / sizeof(x[0])))
#define FORCEINLINE inline __attribute__((always_inline))

inline void debug(const char *f, ...) { va_list a; va_start(a, f); vfprintf(stderr, f, a); va_end(a); }
inline void warning(const char *f, ...) { va_list a; va_start(a, f); vfprintf(stderr, f, a); va_end(a); }
[[noreturn]] inline void error(const char *f, ...) { va_list a; va_start(a, f); vfprintf(stderr, f, a); va_end(a); abort(); }
#define HPL1_UNIMPLEMENTED(f) error("unimplemented: %s\n", #f)

#endif
