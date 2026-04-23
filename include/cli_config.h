/*
 * LinCLI - Framework-wide configuration macros.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_CONFIG_H_
#define _CLI_CONFIG_H_

/* 命令参数全局共享缓冲区大小 */
#ifndef CLI_CMD_BUF_SIZE
#define CLI_CMD_BUF_SIZE 128
#endif

/* 内存池配置 */
#ifndef CLI_MPOOL_COUNT
#define CLI_MPOOL_COUNT 4
#endif

#ifndef CLI_MPOOL_SIZE
#define CLI_MPOOL_SIZE CLI_CMD_BUF_SIZE
#endif

#if CLI_MPOOL_COUNT > 32
#error "CLI_MPOOL_COUNT must not exceed 32"
#endif

#endif
