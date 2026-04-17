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
 * @file test_int_array.c
 * @brief CLI 框架 INT_ARRAY 类型选项测试用例。
 *
 * 注册命令：ta
 * 命令描述：Test INT_ARRAY option with depends
 *
 * 选项列表：
 *   -v, --verbose    BOOL        Enable verbose
 *   -n, --nums       INT_ARRAY   Number list (max 8, depends on verbose)
 *
 * 使用示例：
 *   ta -v -n 1 2 3
 *
 * 预期输出（颜色前缀已省略）：
 *   INT_ARRAY test executed!
 *     verbose = true
 *     nums = 1 2 3
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct array_args {
	bool verbose;
	int *nums;
	size_t nums_count;
};

static int array_handler(void *_args)
{
	struct array_args *args = _args;
	cli_printk("INT_ARRAY test executed!\r\n");
	if (args->verbose)
		cli_printk("  verbose = true\r\n");
	if (args->nums && args->nums_count > 0) {
		cli_printk("  nums = ");
		for (size_t i = 0; i < args->nums_count; i++)
			cli_printk(KERN_NOTICE "%d ", args->nums[i]);
		cli_printk(KERN_NOTICE "\r\n");
	}
	return 0;
}

CLI_COMMAND(ta, "ta", "Test INT_ARRAY option with depends", array_handler,
	    (struct array_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct array_args,
		   verbose),
	    OPTION('n', "nums", INT_ARRAY, "Number list", struct array_args,
		   nums, 8, "verbose"),
	    END_OPTIONS);
