/*
 * Copyright (C) 2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <coldtrace/config.h>
#include <coldtrace/utils.h>
#include <dice/log.h>
#include <dice/module.h>
#include <string.h>

// global configuration
DICE_HIDE uint32_t max_file_count_ = -1;
DICE_HIDE size_t trace_size_       = COLDTRACE_DEFAULT_TRACE_SIZE;
DICE_HIDE bool disable_writes_     = false;
DICE_HIDE char path_[128];
DICE_HIDE char pattern_[128];
#define COLDTRACE_FILE_SUFFIX "/freezer_log_%d_%d.bin"

DICE_MODULE_INIT({
    char *var = getenv("COLDTRACE_PATH");
    if (var == NULL) {
        log_fatal("Set COLDTRACE_PATH to a valid directory\n");
    }
    coldtrace_set_path(var);

    if (ensure_dir_exists(var) != 0)
        abort();

    if (getenv("COLDTRACE_DISABLE_CLEANUP") == NULL)
        if (ensure_dir_empty(var) != 0)
            abort();

    var = getenv("COLDTRACE_MAX_FILES");
    if (var) {
        uint32_t val = strtoul(var, NULL, 10);
        coldtrace_set_max(val);
    }

    var = getenv("COLDTRACE_DISABLE_WRITES");
    if (var && (strcmp(var, "yes") == 0 || strcmp(var, "true") == 0))
        coldtrace_disable_writes();
})

DICE_HIDE void
coldtrace_set_path(const char *path)
{
    if (strlen(path) >= (sizeof(pattern_) - sizeof(COLDTRACE_FILE_SUFFIX))) {
        log_fatal("error: path too long\n");
    }
    strcpy(path_, path);
    strcpy(pattern_, path);
    strcpy(pattern_ + strlen(path), COLDTRACE_FILE_SUFFIX);
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
