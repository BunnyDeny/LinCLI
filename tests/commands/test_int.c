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

#include "cli_config.h"

#if CLI_ENABLE_DEMO_INT
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

CLI_COMMAND(ti, "ti", "Test INT option",
	    USAGE("ti [-n <num>]"),
	    int_handler, (struct int_args *)0,
	    OPTION('n', "num", INT, "Integer value", struct int_args, num, 0,
		   NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_INT */
