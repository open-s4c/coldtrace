/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_LZ4_LIBC_H
#define COLDTRACE_LZ4_LIBC_H

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <stdatomic.h>
#include <stddef.h>

#define COLDTRACE_LZ4_REAL_(NAME)                                             \
    ({                                                                        \
        static _Atomic(void *) cached_;                                       \
        void *p_ = atomic_load_explicit(&cached_, memory_order_acquire);      \
        if (p_ == NULL) {                                                     \
            p_ = dlsym(RTLD_NEXT, (NAME));                                    \
            atomic_store_explicit(&cached_, p_, memory_order_release);        \
        }                                                                     \
        p_;                                                                   \
    })

static inline void *
lz4_real_memcpy(void *dest, const void *src, size_t n)
{
    typedef void *(*fn_t)(void *, const void *, size_t);
    return ((fn_t)COLDTRACE_LZ4_REAL_("memcpy"))(dest, src, n);
}

static inline void *
lz4_real_memmove(void *dest, const void *src, size_t n)
{
    typedef void *(*fn_t)(void *, const void *, size_t);
    return ((fn_t)COLDTRACE_LZ4_REAL_("memmove"))(dest, src, n);
}

static inline void *
lz4_real_memset(void *ptr, int value, size_t n)
{
    typedef void *(*fn_t)(void *, int, size_t);
    return ((fn_t)COLDTRACE_LZ4_REAL_("memset"))(ptr, value, n);
}

#define LZ4_memcpy(dst, src, size)  lz4_real_memcpy((dst), (src), (size))
#define LZ4_memmove(dst, src, size) lz4_real_memmove((dst), (src), (size))
#define LZ4_memset(dst, c, size)    lz4_real_memset((dst), (c), (size))

#endif /* COLDTRACE_LZ4_LIBC_H */
