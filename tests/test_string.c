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

CLI_COMMAND(ts, "ts", "Test STRING option", string_handler,
	    (struct string_args *)0,
	    OPTION('m', "msg", STRING, "Message text", struct string_args, msg),
	    END_OPTIONS);
CMD_ALIAS(echo, "ts --msg", "print the string");
