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

#ifdef CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct double_args {
	double factor;
};

static int double_handler(void *_args)
{
	struct double_args *args = _args;
	cli_printk("DOUBLE test executed!\r\n");
	cli_printk("  factor = %f\r\n", args->factor);
	return 0;
}

CLI_COMMAND(td, "td", "Test DOUBLE option", double_handler,
	    (struct double_args *)0,
	    OPTION('f', "factor", DOUBLE, "Float value", struct double_args, factor, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
