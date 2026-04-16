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
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct double_args {
	double factor;
};

static int double_handler(void *_args)
{
	struct double_args *args = _args;
	cli_printk("DOUBLE test executed!\n");
	cli_printk("  factor = %f\n", args->factor);
	return 0;
}

CLI_COMMAND(td, "td", "Test DOUBLE option", double_handler,
	    (struct double_args *)0,
	    OPTION('f', "factor", DOUBLE, "Float value", struct double_args,
		   factor),
	    END_OPTIONS);
