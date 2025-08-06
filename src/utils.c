/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/utils.h>
#include <dice/compiler.h>
#include <dice/log.h>
#include <dirent.h>
#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


DICE_HIDE bool
has_ext(const char *fname, const char *ext)
{
    const char *base = strrchr(fname, '.');
    return base && strcmp(base, ext) == 0;
}

DICE_HIDE int
ensure_dir_empty(const char *path)
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
        if (!has_ext(entry->d_name, ".bin"))
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

DICE_HIDE int
ensure_dir_exists(const char *path)
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
