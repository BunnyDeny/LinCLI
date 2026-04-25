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
 * @file test_callback.c
 * @brief CLI 框架 CALLBACK 类型选项测试用例。
 *
 * 注册命令：tc
 * 命令描述：Test CALLBACK option
 *
 * 选项列表：
 *   -c, --cfg    CALLBACK    Raw config string
 *
 * 使用示例：
 *   tc -c foo
 *
 * 预期输出（颜色前缀已省略）：
 *   CALLBACK test executed!
 *     custom callback triggered with: foo
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct cb_args {
	const char *raw;
};

static int callback_handler(void *_args)
{
	struct cb_args *args = _args;
	cli_printk("CALLBACK test executed!\r\n");
	cli_printk("  custom callback triggered with: %s\r\n",
		   args->raw ? args->raw : "(null)");
	return 0;
}

CLI_COMMAND(tc, "tc", "Test CALLBACK option", callback_handler,
	    (struct cb_args *)0,
	    OPTION('c', "cfg", CALLBACK, "Raw config string", struct cb_args, raw, 0, NULL, NULL, false),
	    END_OPTIONS);
