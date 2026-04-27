/*
 * LinCLI - Memory pool for framework internal use.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_MPOOL_H_
#define _CLI_MPOOL_H_

#include <stdint.h>
#include <stddef.h>
#include "cli_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void *cli_mpool_alloc_base(void);
void cli_mpool_free(void *ptr);
void cli_mpool_get_usage(const char **owners, int *used_count);
void cli_mpool_dump_usage(void);

#define cli_mpool_alloc() cli_mpool_alloc_caller(__func__)
void *cli_mpool_alloc_caller(const char *caller);

#ifdef __cplusplus
}
#endif

#endif
