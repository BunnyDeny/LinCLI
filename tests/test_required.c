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
 * @file test_required.c
 * @brief CLI 框架 OPTION_7（基础类型 + required）测试用例。
 *
 * 注册命令：tr
 * 命令描述：Test required option
 *
 * 选项列表：
 *   -f, --file    STRING [必需]    Input file path
 *
 * 使用示例：
 *   tr -f /tmp/data.txt    -> 正常
 *   tr                     -> 报错：缺少必需选项
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct required_args {
	char *file;
};

static int required_handler(void *_args)
{
	struct required_args *args = _args;
	cli_printk("REQUIRED test executed!\r\n");
	if (args->file)
		cli_printk("  file = %s\r\n", args->file);
	return 0;
}

CLI_COMMAND(tr, "tr", "Test required option", required_handler,
	    (struct required_args *)0,
	    OPTION('f', "file", STRING, "Input file path", struct required_args,
		   file, 0, NULL, NULL, true),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
