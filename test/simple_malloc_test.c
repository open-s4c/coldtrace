/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * SPDX-License-Identifier: 0BSD
 */
#include <assert.h>
#include <stdlib.h>

int
main(void)
{
    void *ptr1 = malloc(1024);
    void *ptr2 = malloc(1024);

    (void)ptr1;
    free(ptr2);

    return 0;
}
