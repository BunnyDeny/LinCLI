/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_AUTO_CMD_H_
#define _CLI_AUTO_CMD_H_

/**
 * @brief 开机自动执行命令表（弱定义）。
 *
 * 用户可在自己的工程中重新定义此数组和计数变量，
 * 调度器在初始化完毕后会按顺序自动执行这些命令。
 *
 * 示例：
 *   const char * const cli_auto_cmds[] = {
 *       "tb -v",
 *       "ti -n 42",
 *   };
 *   const int cli_auto_cmds_count = sizeof(cli_auto_cmds) / sizeof(cli_auto_cmds[0]);
 */
__attribute__((weak)) extern const char *const cli_auto_cmds[];
__attribute__((weak)) extern const int cli_auto_cmds_count;

#endif
