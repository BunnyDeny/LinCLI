/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_ERRNO_COMMON_H_
#define _CLI_ERRNO_COMMON_H_

/* ========== 通用错误码 (-1 ~ -9) ========== */
#define CLI_OK 0
#define CLI_ERR_NULL -1 /* 空指针入参 */
#define CLI_ERR_NOTFOUND -2 /* 查找失败（状态、命令、选项等） */
#define CLI_ERR_NOMEM -3 /* 内存/缓冲区不足 */
#define CLI_ERR_INVAL -4 /* 非法参数 */

#endif
