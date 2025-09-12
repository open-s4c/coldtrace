/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>

int
main(void)
{
    void *ptr1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS ,0, 0);

    munmap(ptr1, 1);
    return 0;
}
