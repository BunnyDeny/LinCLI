/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file test_buf_insufficient.c
 * @brief 测试 CLI_COMMAND 与 CLI_COMMAND_WITH_BUF 在缓冲区不足时的行为。
 *
 * 本文件包含两个独立的命令：
 *
 * 1. big1 —— 使用默认共享缓冲区 g_cli_cmd_buf（大小为 CLI_CMD_BUF_SIZE）
 *    结构体仅包含一个 char[CLI_CMD_BUF_SIZE] 数组，因此结构体大小恰好等于
 *    CLI_CMD_BUF_SIZE，剩余空间为 0。注册了 1 个 BOOL 选项后，opt_seen
 *    需要 1 字节，0 < 1，会触发 "命令 big1 缓冲区不足，缺少 1 字节"。
 *
 * 2. big2 —— 使用 CLI_COMMAND_WITH_BUF 指定独立缓冲区 big2_buf（1 B）
 *    结构体仅包含一个 char[1]，大小为 1，剩余空间为 0。注册了 1 个选项后，
 *    opt_seen 需要 1 字节，0 < 1，同样会触发 "命令 big2 缓冲区不足，缺少 1 字节"。
 *
 * 预期终端输出：
 *   lin@linCli> big1
 *   [ERR] 命令 big1 缓冲区不足，缺少 1 字节
 *   [ERR] 命令解析失败: big1
 *   ...
 *
 *   lin@linCli> big2
 *   [ERR] 命令 big2 缓冲区不足，缺少 1 字节
 *   [ERR] 命令解析失败: big2
 *   ...
 */

#include "cmd_dispose.h"
#include "cli_io.h"

/* ============================================================
 * big1：使用 CLI_COMMAND（共享 g_cli_cmd_buf）
 * ============================================================ */

struct big1_args {
	char padding[CLI_CMD_BUF_SIZE];
};

static int big1_handler(void *_args)
{
	(void)_args;
	cli_printk("big1 should never be executed!\r\n");
	return 0;
}

CLI_COMMAND(big1, "big1", "Test insufficient shared buffer", big1_handler,
	    (struct big1_args *)0,
	    OPTION('a', "a", BOOL, "A", struct big1_args, padding),
	    END_OPTIONS);

/* ============================================================
 * big2：使用 CLI_COMMAND_WITH_BUF（独立缓冲区 1 B）
 * ============================================================ */

struct big2_args {
	char padding[1];
};

static char big2_buf[1];

static int big2_handler(void *_args)
{
	(void)_args;
	cli_printk("big2 should never be executed!\r\n");
	return 0;
}

CLI_COMMAND_WITH_BUF(big2, "big2", "Test insufficient private buffer",
		     big2_handler, (struct big2_args *)0, big2_buf,
		     sizeof(big2_buf),
		     OPTION('x', "x", BOOL, "X", struct big2_args, padding),
		     END_OPTIONS);
