/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */
#include <coldtrace/config.h>
#include <coldtrace/utils.h>
#include <dice/chains/capture.h>
#include <dice/events/thread.h>
#include <dice/module.h>
#include <dice/self.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vsync/spinlock/caslock.h>

#define MAX_LINE_LENGTH 256
#define MAX_PATH_LENGTH 1024

static int mkdir_(const char *path);
static int cp_(const char *src, const char *dest);

static int
copy_proc_maps_(const char *path)
{
    pid_t pid = getpid();
    char src[MAX_PATH_LENGTH];
    char dst[MAX_PATH_LENGTH];

    snprintf(src, sizeof(src), "/proc/%d/maps", pid);
    snprintf(dst, sizeof(dst), "%s/maps", path);

    if (mkdir_(path) != 0)
        return -1;
    if (cp_(src, dst) != 0)
        return -1;

    return 0;
}

static int
copy_mapped_files_(const char *path)
{
    char proc_maps[MAX_PATH_LENGTH];
    char line[MAX_LINE_LENGTH];
    char dst[MAX_PATH_LENGTH];
    char *last_slash;
    char unique[MAX_PATH_LENGTH * 1000] = {0};

    // Open proc maps file in path.
    snprintf(proc_maps, sizeof(proc_maps), "%s/maps", path);
    FILE *file = fopen(proc_maps, "r");
    if (!file) {
        perror("Failed to open proc map file");
        return -1;
    }

    // For each line in proc maps, copy mapped file if exists.
    while (fgets(line, sizeof(line), file)) {
        // Find file path at the end of the line.
        char *src = strchr(line, '/');
        if (!src)
            continue;

        // Remove newline if present.
        src[strcspn(src, "\n")] = 0;

        // Skip duplicates.
        if (strstr(unique, src))
            continue;

        // skip files with .bin extension
        if (has_ext(src, ".bin"))
            continue;

        // Create the destination path.
        snprintf(dst, sizeof(dst), "%s%s", path, src);
        last_slash = strrchr(dst, '/');
        if (last_slash) {
            // Directory is up to the last slash. Remove it, create directory,
            // and then restore slash.
            *last_slash = '\0';
            if (mkdir_(dst) != 0)
                return -1;
            *last_slash = '/';
        }

        if (cp_(src, dst) != 0)
            return -1;

        // Mark this file as copied.
        strcat(unique, ":");
        strcat(unique, src);
    }

    fclose(file);
    return 0;
}

/* -----------------------------------------------------------------------------
 * subscriber callback
 * -------------------------------------------------------------------------- */

static bool maps_copied_ = false;
static caslock_t lock_;

DICE_MODULE_INIT({
    if (getenv("COLDTRACE_DISABLE_COPY")) {
        maps_copied_ = true;
    }
})

static inline void
copy_maps_and_mapped_files_(void)
{
    maps_copied_ = true;

    log_info("copy procmaps");

    if (copy_proc_maps_(coldtrace_get_path()) != 0)
        abort();
    if (copy_mapped_files_(coldtrace_get_path()) != 0)
        abort();
}

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_SELF_INIT, {
    caslock_acquire(&lock_);

    // copy on main thread's init might be too early to get useful information
    if (maps_copied_ || self_id(md) == MAIN_THREAD) {
        goto out;
    }

    copy_maps_and_mapped_files_();

out:
    caslock_release(&lock_);
    return PS_OK;
})

PS_SUBSCRIBE(CAPTURE_EVENT, EVENT_SELF_FINI, {
    caslock_acquire(&lock_);
    if (maps_copied_) {
        goto out;
    }

    copy_maps_and_mapped_files_();

out:
    caslock_release(&lock_);
    return PS_OK;
})

/* -----------------------------------------------------------------------------
 * helper functions
 * --------------------------------------------------------------------------
 */

static int
mkdir_(const char *path)
{
    char temp[MAX_PATH_LENGTH];
    char *p = NULL;
    size_t len;

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    if (temp[len - 1] == '/') {
        temp[len - 1] = 0;
    }

    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(temp, S_IRWXU) && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    return mkdir(temp, S_IRWXU) && errno != EEXIST ? -1 : 0;
}

static int
cp_(const char *src, const char *dst)
{
    FILE *fsrc = fopen(src, "rb");
    if (!fsrc) {
        perror("fopen");
        return -1;
    }

    FILE *fdst = fopen(dst, "wb");
    if (!fdst) {
        fclose(fsrc);
        perror("fopen");
        return -1;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fsrc)) > 0) {
        fwrite(buffer, 1, bytes, fdst);
    }

    fclose(fsrc);
    fclose(fdst);
    return 0;
}
