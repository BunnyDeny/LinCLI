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
 * @file test_string.c
 * @brief CLI 框架 STRING 类型选项测试用例。
 *
 * 注册命令：ts
 * 命令描述：Test STRING option
 *
 * 选项列表：
 *   -m, --msg    STRING    Message text
 *
 * 使用示例：
 *   ts -m hello
 *
 * 预期输出（颜色前缀已省略）：
 *   STRING test executed!
 *     msg = hello
 */

#include "cli_config.h"

#if CLI_ENABLE_DEMO_STRING
#include "cmd_dispose.h"
#include "cli_io.h"

struct string_args {
	char *msg;
};

static int string_handler(void *_args)
{
	struct string_args *args = _args;
	if (args->msg)
		cli_printk(" %s\r\n", args->msg);
	return 0;
}

CLI_COMMAND(ts, "ts", "Test STRING option",
	    USAGE("ts [-m <msg>]"),
	    string_handler, (struct string_args *)0,
	    OPTION('m', "msg", STRING, "Message text", struct string_args, msg,
		   0, NULL, NULL, false),
	    END_OPTIONS);
#endif /* CLI_ENABLE_DEMO_STRING */
