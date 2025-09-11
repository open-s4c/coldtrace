/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <coldtrace/config.h>
#include <coldtrace/utils.h>
#include <dice/module.h>
#include <string.h>

// global configuration
DICE_HIDE uint32_t _max_file_count = -1;
DICE_HIDE size_t _trace_size       = COLDTRACE_DEFAULT_SIZE;
DICE_HIDE bool _disable_writes     = false;
DICE_HIDE char _path[128];
#define COLDTRACE_FILE_SUFFIX "/freezer_log_%d_%d.bin"


// This initializer has to run before other hooks in coldtrace so that the path
// is properly initialized. Therefore, we set DICE_MODULE_PRIO to lower than the
// rest of the modules.
static char *__path;

DICE_HIDE const char *
coldtrace_path(void)
{
    return __path;
}

DICE_MODULE_INIT({
    __path = getenv("COLDTRACE_PATH");
    if (_path == NULL) {
        log_printf("Set COLDTRACE_PATH to a valid directory\n");
        exit(EXIT_FAILURE);
    }

    if (ensure_dir_exists(__path) != 0)
        abort();

    if (getenv("COLDTRACE_DISABLE_CLEANUP") == NULL)
        if (ensure_dir_empty(__path) != 0)
            abort();

    coldtrace_set_path(__path);

    char *var = getenv("COLDTRACE_MAX_FILES");
    if (var) {
        uint32_t val = strtoul(var, NULL, 10);
        coldtrace_set_max(val);
    }

    if (getenv("COLDTRACE_DISABLE_WRITES"))
        coldtrace_disable_writes();
})

DICE_HIDE void
coldtrace_disable_writes(void)
{
    _disable_writes = true;
}
DICE_HIDE void
coldtrace_set_path(const char *path)
{
    if (strlen(path) >= (128 - sizeof(COLDTRACE_FILE_SUFFIX))) {
        log_printf("error: path too long\n");
        exit(EXIT_FAILURE);
    }
    strcpy(_path, path);
    strcpy(_path + strlen(path), COLDTRACE_FILE_SUFFIX);
}

DICE_HIDE void
coldtrace_set_size(size_t size)
{
    _trace_size = size;
}

DICE_HIDE void
coldtrace_set_max(uint32_t max_file_count)
{
    _max_file_count = max_file_count;
}
