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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vsync/atomic.h>

vatomic64_t next_alloc_index;
vatomic64_t next_atomic_index;


DICE_HIDE bool
has_ext_(const char *fname, const char *ext)
{
    const char *base = strrchr(fname, '.');
    return base && strcmp(base, ext) == 0;
}

static int
_ensure_dir_empty(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    struct dirent *entry;
    char filepath[4096];

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Check for .bin extension
        if (!has_ext_(entry->d_name, ".bin"))
            continue;

        // Build full path
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        // Check if it's a regular file
        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (remove(filepath) != 0) {
                perror(filepath);
                return -1;
            }
        }
    }

    closedir(dir);
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
// is properly initialized. Therefore, we set DICE_MODULE_PRIO to lower than the
// rest of the modules.
static const char *_path;

DICE_HIDE const char *
coldtrace_path(void)
{
    return _path;
}

DICE_MODULE_INIT({
    _path = getenv("COLDTRACE_PATH");
    if (_path == NULL) {
        log_printf("Set COLDTRACE_PATH to a valid directory\n");
        exit(EXIT_FAILURE);
    }

    if (_ensure_dir_exists(_path) != 0)
        abort();

    if (getenv("COLDTRACE_DISABLE_CLEANUP") == NULL)
        if (_ensure_dir_empty(_path) != 0)
            abort();

    coldtrace_set_path(_path);

    char *var = getenv("COLDTRACE_MAX_FILES");
    if (var) {
        uint32_t val = strtoul(var, NULL, 10);
        coldtrace_set_max(val);
    }

    if (getenv("COLDTRACE_DISABLE_WRITES"))
        coldtrace_disable_writes();
})
