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

void *cli_mpool_alloc(void);
void cli_mpool_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
