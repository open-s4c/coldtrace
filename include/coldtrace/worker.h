/*
 * Copyright (C) 2026 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#ifndef COLDTRACE_WORKER_H
#define COLDTRACE_WORKER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// Fd number the worker finds its end of the control socket on.
#define WORKER_CTRL_FD 3

// Argv layout passed to coldtrace-worker:
//   argv[1] = file pattern  (e.g. "<path>/freezer_log_%d_%d.bin")
//   argv[2] = trace size    (size_t)
#define WORKER_ARG_PATTERN 1
#define WORKER_ARG_SIZE    2
#define WORKER_ARGC        3

// ---- control socket protocol ----------------------------------------------
//
// OPEN: sent by a traced thread on its first trace.
//   payload:   struct worker_open_msg
//   ancillary: exactly one fd (SCM_RIGHTS) -- the thread's channel memfd

#define WORKER_MSG_OPEN 1u

struct worker_open_msg {
    uint32_t type;     // WORKER_MSG_OPEN
    uint32_t reserved; // keeps tid 8-byte aligned
    uint64_t tid;      // dice thread id (self_id) of the producer
};

// False once the worker is known to be gone.
bool coldtrace_worker_alive(void);

// Send OPEN {tid} carrying `fd` to the worker.
// Returns false if there is no worker or the send failed.
bool coldtrace_worker_send_open(uint64_t tid, int fd);

// Called by the main thread at exit, after its last fragment was handed off.
// Tells the worker no more channels will come, then waits up to
// coldtrace_get_worker_timeout() ms for it to write everything and exit.
void coldtrace_worker_finish(void);

#endif // COLDTRACE_WORKER_H
