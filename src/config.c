/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <coldtrace/config.h>
#include <coldtrace/utils.h>
#include <dice/log.h>
#include <dice/module.h>
#include <string.h>

#define DEFAULT_PATH_SIZE 128

// global configuration
DICE_HIDE uint32_t max_file_count_ = -1;
DICE_HIDE size_t trace_size_       = COLDTRACE_DEFAULT_TRACE_SIZE;
DICE_HIDE bool disable_writes_     = false;
DICE_HIDE char path_[DEFAULT_PATH_SIZE];
DICE_HIDE char pattern_[DEFAULT_PATH_SIZE];
#define COLDTRACE_FILE_SUFFIX "/freezer_log_%d_%d.bin"
#define COLDTRACE_DISABLE_ON(cond, msg, ...)                                   \
    do {                                                                       \
        if (cond) {                                                            \
            log_warn(msg ", disabling coldtrace writes", ##__VA_ARGS__);       \
            coldtrace_disable_writes();                                        \
            return true;                                                       \
        }                                                                      \
    } while (0)

DICE_MODULE_INIT({
    char *var = getenv("COLDTRACE_MAX_FILES");
    if (var) {
        uint32_t val = strtoul(var, NULL, 10);
        coldtrace_set_max(val);
    }

    var = getenv("COLDTRACE_DISABLE_WRITES");
    if (var && (strcmp(var, "yes") == 0 || strcmp(var, "true") == 0))
        coldtrace_disable_writes();

    var = getenv("COLDTRACE_TRACE_SIZE");
    if (var) {
        coldtrace_set_trace_size(strtoull(var, NULL, 10));
    }

    var = getenv("COLDTRACE_PATH");
    COLDTRACE_DISABLE_ON(var == NULL,
                         "Set COLDTRACE_PATH to a valid directory");

    COLDTRACE_DISABLE_ON(!coldtrace_set_path(var),
                         "Failed to set COLDTRACE_PATH");

    COLDTRACE_DISABLE_ON(
        ensure_dir_exists(var) != 0,
        "COLDTRACE_PATH '%s' does not exist and cannot be created", var);

    if (getenv("COLDTRACE_DISABLE_CLEANUP") == NULL)
        COLDTRACE_DISABLE_ON(ensure_dir_empty(var) != 0,
                             "COLDTRACE_PATH '%s' is not empty, "
                             "the directory must be empty",
                             var);
})

DICE_HIDE bool
coldtrace_set_path(const char *path)
{
    if (strlen(path) >= (sizeof(pattern_) - sizeof(COLDTRACE_FILE_SUFFIX))) {
        log_warn("error: path too long\n");
        return false;
    }
    strcpy(path_, path);
    strcpy(pattern_, path);
    strcpy(pattern_ + strlen(path), COLDTRACE_FILE_SUFFIX);
    return true;
}

DICE_HIDE const char *
coldtrace_get_path(void)
{
    return path_;
}

DICE_HIDE const char *
coldtrace_get_file_pattern(void)
{
    return pattern_;
}

DICE_HIDE void
coldtrace_disable_writes(void)
{
    disable_writes_ = true;
}

DICE_HIDE bool
coldtrace_writes_disabled(void)
{
    return disable_writes_;
}

DICE_HIDE void
coldtrace_set_trace_size(size_t size)
{
    trace_size_ = size;
}

DICE_HIDE size_t
coldtrace_get_trace_size(void)
{
    return trace_size_;
}

DICE_HIDE void
coldtrace_set_max(uint32_t max_file_count)
{
    max_file_count_ = max_file_count;
}

DICE_HIDE uint32_t
coldtrace_get_max()
{
    return max_file_count_;
}
