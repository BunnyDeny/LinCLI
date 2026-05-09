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
 * @file test_double.c
 * @brief CLI 框架 DOUBLE 类型选项测试用例。
 *
 * 注册命令：td
 * 命令描述：Test DOUBLE option
 *
 * 选项列表：
 *   -f, --factor    DOUBLE    Float value
 *
 * 使用示例：
 *   td -f 3.14
 *
 * 预期输出（颜色前缀已省略）：
 *   DOUBLE test executed!
 *     factor = 3.140000
 * 注意，链接器如果配置了-specs=nano.specs选项，会导致
 * 无法格式化打印float,double等浮点数，会导致本测试用例
 * 无法正常打印factor的值，此时要么去掉-specs=nano.specs
 * 选项，要么直接调试程序看factor对应的值是否和预期对应即
 * 可
 */
#include "cli_config.h"

#if CLI_ENABLE_DEMO_DOUBLE
#include "cmd_dispose.h"
#include "cli_io.h"
#include "cli_float.h"

struct float_args {
	float factor;
};

static int float_handler(void *_args)
{
	struct float_args *args = _args;
	char buf[32];
	cli_ftoa(args->factor, buf, sizeof(buf), 6);
	cli_printk("FLOAT test executed!\r\n");
	cli_printk("  factor = %s\r\n", buf);
	return 0;
}

CLI_COMMAND(td, "td", "Test FLOAT option",
	    USAGE("td [-f <factor>]"),
	    float_handler, (struct float_args *)0,
	    OPTION('f', "factor", FLOAT, "Float value", struct float_args,
		   factor, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_DOUBLE */
