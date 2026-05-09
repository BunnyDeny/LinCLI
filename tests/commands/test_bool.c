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

#if CLI_ENABLE_DEMO_BOOL
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

CLI_COMMAND(tb, "tb", "Test BOOL option",
	    USAGE("tb [-v]"),
	    bool_handler, (struct bool_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct bool_args,
		   verbose, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_BOOL */
