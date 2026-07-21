/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef COLDTRACE_SUBS_UTILS_H
#define COLDTRACE_SUBS_UTILS_H

#define COLDTRACE_APPEND_OR_RETURN_OK(out, md, kind, value)                    \
    do {                                                                       \
        void *coldtrace_tmp_ = coldtrace_thread_append((md), (kind), (value)); \
        if (coldtrace_tmp_ == NULL) {                                          \
            return PS_OK;                                                      \
        }                                                                      \
        (out) = coldtrace_tmp_;                                                \
    } while (0)

#endif /* COLDTRACE_SUBS_UTILS_H */
