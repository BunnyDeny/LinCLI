/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
