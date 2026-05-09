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

#include "cli_config.h"

#if CLI_ENABLE_DEMO_CALLBACK
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

CLI_COMMAND(tc, "tc", "Test CALLBACK option",
	    USAGE("tc [-c <cfg>]"),
	    callback_handler, (struct cb_args *)0,
	    OPTION('c', "cfg", CALLBACK, "Raw config string", struct cb_args,
		   raw, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_CALLBACK */
