#ifndef PADDED_TYPES_H
#define PADDED_TYPES_H

#include "misc/bsd_stdatomic.h"//Until c11 stdatoic.h is available

#define CACHE_LINE_SIZE 64

#define CACHE_LINE_SIZE_PAD(size) CACHE_LINE_SIZE - (size) % CACHE_LINE_SIZE

#ifndef _ISOC11_SOURCE
#    if _POSIX_C_SOURCE >= 200112
#        include <malloc.h>
#        define aligned_alloc memalign
#    endif
#endif


typedef union {
    volatile atomic_flag value;
    char padding[CACHE_LINE_SIZE];
} LLPaddedFlag;

typedef union {
    volatile atomic_bool value;
    char padding[CACHE_LINE_SIZE];
} LLPaddedBool;

typedef union {
    volatile atomic_int value;
    char padding[CACHE_LINE_SIZE];
} LLPaddedInt;

typedef union {
    volatile atomic_uint value;
    char padding[CACHE_LINE_SIZE];
} LLPaddedUInt;

typedef union {
    volatile atomic_ulong value;
    char padding[CACHE_LINE_SIZE];
} LLPaddedULong;

typedef union {
    volatile atomic_intptr_t value;
    char padding[CACHE_LINE_SIZE];
} LLPaddedPointer;

typedef union {
    volatile double value;
    char padding[CACHE_LINE_SIZE];
} LLPaddedDouble;

#endif
