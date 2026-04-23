/*
 * LinCLI - Memory pool for CLI internal use.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CLI_MPOOL_H
#define _CLI_MPOOL_H

#include <stdint.h>
#include <stddef.h>

#ifndef CLI_MPOOL_COUNT
#define CLI_MPOOL_COUNT 4
#endif

#ifndef CLI_MPOOL_SIZE
#define CLI_MPOOL_SIZE 256
#endif

#if CLI_MPOOL_COUNT > 32
#error "CLI_MPOOL_COUNT must not exceed 32"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void *cli_mpool_alloc(void);
void cli_mpool_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
