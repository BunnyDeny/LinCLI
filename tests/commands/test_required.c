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

#if CLI_ENABLE_DEMO_REQUIRED
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

CLI_COMMAND(tr, "tr", "Test required option",
	    USAGE("tr -f <file>"),
	    required_handler, (struct required_args *)0,
	    OPTION('f', "file", STRING, "Input file path", struct required_args,
		   file, 0, NULL, NULL, true),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_REQUIRED */
