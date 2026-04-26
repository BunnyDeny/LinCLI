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
 * @file test_int.c
 * @brief CLI 框架 INT 类型选项测试用例。
 *
 * 注册命令：ti
 * 命令描述：Test INT option
 *
 * 选项列表：
 *   -n, --num    INT    Integer value
 *
 * 使用示例：
 *   ti -n 42
 *
 * 预期输出（颜色前缀已省略）：
 *   INT test executed!
 *     num = 42
 */

#ifdef CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct int_args {
	int num;
};

static int int_handler(void *_args)
{
	struct int_args *args = _args;
	cli_printk("INT test executed!\r\n");
	cli_printk("  num = %d\r\n", args->num);
	return 0;
}

CLI_COMMAND(ti, "ti", "Test INT option", int_handler, (struct int_args *)0,
	    OPTION('n', "num", INT, "Integer value", struct int_args, num, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
