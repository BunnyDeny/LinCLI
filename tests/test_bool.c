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
 * @file test_bool.c
 * @brief CLI 框架 BOOL 类型选项测试。
 *
 * 注册命令：tb
 * 命令描述：Test BOOL option
 *
 * 选项列表：
 *   -v, --verbose    BOOL    Enable verbose
 *
 * 使用示例：
 *   tb -v
 *
 * 预期输出（颜色前缀已省略）：
 *   BOOL test executed!
 *     verbose = true
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct bool_args {
	bool verbose;
};

static int bool_handler(void *_args)
{
	struct bool_args *args = _args;
	cli_printk("BOOL test executed!\r\n");
	(args->verbose) ? cli_printk("  verbose = true\r\n") :
			  cli_printk("  verbose = false\r\n");
	return 0;
}

CLI_COMMAND(tb, "tb", "Test BOOL option", bool_handler, (struct bool_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct bool_args,
		   verbose, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
