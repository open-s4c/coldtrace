/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef COLDTRACE_LOGS_H
#define COLDTRACE_LOGS_H
#include <dice/log.h>

#undef log_fatal

#if defined(__OHOS__)
    #define LOG_FATAL_TERMINATE() abort()
#else
    #define LOG_FATAL_TERMINATE() exit(EXIT_FAILURE)
#endif

/* log_fatal prints the message and exits with EXIT_FAILURE if the system is not
 * OHOS. If the system is OHOS, it prints the message and aborts the program.
 *
 * It indicates a fatal error was detected, but it is a possibly expected error
 */
#define log_fatal(fmt, ...)                                                    \
    do {                                                                       \
        LOG_LOCK_ACQUIRE;                                                      \
        log_printf(LOG_PREFIX);                                                \
        log_printf(fmt, ##__VA_ARGS__);                                        \
        log_printf(LOG_SUFFIX);                                                \
        LOG_LOCK_RELEASE;                                                      \
        LOG_FATAL_TERMINATE();                                                 \
    } while (0)

#endif // COLDTRACE_LOGS_H
