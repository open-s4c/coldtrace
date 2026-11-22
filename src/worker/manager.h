/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 *
 * Manager thread: owns the control socket. Receives OPEN messages from
 * traced threads and starts one consumer per channel.
 */

#ifndef COLDTRACE_WORKER_MANAGER_H
#define COLDTRACE_WORKER_MANAGER_H

#include <pthread.h>

void *manager_thread_(void *arg);

#endif // COLDTRACE_WORKER_MANAGER_H
