/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include "writer.h"

#include <dice/module.h>
#include <dice/self.h>
#include <dice/thread_id.h>
#include <dirent.h>
#include <ftw.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vsync/atomic.h>

vatomic64_t next_alloc_index;
vatomic64_t next_atomic_index;

static int
_unlink_cb(const char *fpath, const struct stat *sb, int typeflag,
           struct FTW *ftwbuf)
{
    int rv = remove(fpath);
    if (rv)
        perror(fpath);

    return rv;
}

static int
_ensure_dir_empty(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return nftw(path, _unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
    else
        return 0;
}

static int
_ensure_dir_exists(const char *path)
{
    struct stat st;

    // Check if the directory exists
    if (stat(path, &st) == 0) {
        // Check if it is actually a directory
        if (S_ISDIR(st.st_mode)) {
            return 0; // Exists
        } else {
            log_printf("not a directory: %s\n", path);
            return -1;
        }
    }

    // Try to create the directory
    if (mkdir(path, 0755) != 0) {
        perror("mkdir failed");
        return -1;
    }

    return 0; // Created successfully
}

// This initializer has to run before other hooks in coldtrace so that the path
// is properly initialized. Therefore, we set DICE_XTOR_PRIO to lower than the
// rest of the modules.
static bool _initd = false;
static const char *_path;

DICE_HIDE const char *
coldtrace_path(void)
{
    return _path;
}

DICE_MODULE_INIT({
    if (_initd) {
        return;
    }

    _path = getenv("COLDTRACE_PATH");
    if (_path == NULL) {
        log_printf("Set COLDTRACE_PATH to a valid directory\n");
        exit(EXIT_FAILURE);
    }

    if (_ensure_dir_empty(_path) != 0)
        abort();

    if (_ensure_dir_exists(_path) != 0)
        abort();

    coldtrace_config(_path);
    _initd = true;
})
