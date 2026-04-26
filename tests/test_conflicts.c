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
 * @file test_conflicts.c
 * @brief CLI 框架 conflicts（互斥）选项测试用例。
 *
 * 注册命令：tcf
 * 命令描述：Test INT_ARRAY option with conflicts (!verbose)
 *
 * 选项列表：
 *   -v, --verbose    BOOL        Enable verbose
 *   -n, --nums       INT_ARRAY   Number list (max 8, conflicts with verbose)
 *
 * 使用示例：
 *   tcf -n 1 2 3     -> 正常
 *   tcf -v           -> 正常
 *   tcf -v -n 1 2 3  -> 报错：选项 -n/--nums 与 verbose 互斥，不能同时使用
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct conflicts_args {
	bool verbose;
	int *nums;
	size_t nums_count;
};

static int conflicts_handler(void *_args)
{
	struct conflicts_args *args = _args;
	cli_printk("CONFLICTS test executed!\r\n");
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

CLI_COMMAND(tcf, "tcf", "Test INT_ARRAY option with conflicts",
	    conflicts_handler, (struct conflicts_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose",
		   struct conflicts_args, verbose, 0, NULL, NULL, false),
	    OPTION('n', "nums", INT_ARRAY, "Number list", struct conflicts_args,
		   nums, 8, NULL, "verbose", false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
