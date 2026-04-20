/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_ERRNO_IO_H_
#define _CLI_ERRNO_IO_H_

#include "cli_errno_common.h"

/* ========== cli_io / FIFO 模块 (-40 ~ -49) ========== */
#define CLI_ERR_FIFO_FULL     -40   /* FIFO 已满 */
#define CLI_ERR_FIFO_EMPTY    -41   /* FIFO 已空 */
#define CLI_ERR_IO_SYNC       -42   /* IO 同步失败 */

#endif
