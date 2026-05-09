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

#if CLI_ENABLE_DEMO_CONFLICTS
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
	    USAGE("tcf [-v] [-n <nums...>]"),
	    conflicts_handler, (struct conflicts_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose",
		   struct conflicts_args, verbose, 0, NULL, NULL, false),
	    OPTION('n', "nums", INT_ARRAY, "Number list", struct conflicts_args,
		   nums, 8, NULL, "verbose", false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_CONFLICTS */
