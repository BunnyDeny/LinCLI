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

#include "cli_config.h"

#if CLI_ENABLE_DEMO_INT_ARRAY
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

CLI_COMMAND(ta, "ta", "Test INT_ARRAY option with depends",
	    USAGE("ta [-v] [-n <nums...>]"),
	    array_handler, (struct array_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct array_args,
		   verbose, 0, NULL, NULL, false),
	    OPTION('n', "nums", INT_ARRAY, "Number list", struct array_args,
		   nums, 8, "verbose", NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_INT_ARRAY */
