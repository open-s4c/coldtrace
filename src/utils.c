/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/utils.h>
#include <dice/compiler.h>
#include <dice/log.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 4096
#define DIR_PERMISSIONS                                                        \
    (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

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
        log_info("opendir: %s", strerror(errno));
        return -1;
    }

    struct dirent *entry;
    char filepath[MAX_PATH_LENGTH];

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check for .bin extension
        if (!has_ext(entry->d_name, ".bin")) {
            continue;
        }

        // Build full path
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        // Check if it's a regular file
        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (remove(filepath) != 0) {
                log_info("%s %s", filepath, strerror(errno));
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
}

#define MAX_PATH_LENGTH 1024
static int
mkdir_(const char *path, unsigned int mode)
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
            if (mkdir(temp, mode) && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    return mkdir(temp, mode) && errno != EEXIST ? -1 : 0;
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
        }

        log_info("not a directory: %s\n", path);
        return -1;
    }

    // Try to create the directory
    if (mkdir_(path, DIR_PERMISSIONS) != 0) {
        log_info("mkdir failed: %s", strerror(errno));
        return -1;
    }

    return 0; // Created successfully
}