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
 * @file test_with_buf.c
 * @brief CLI_COMMAND_WITH_BUF 宏测试用例。
 *
 * 注册命令：tw
 * 命令描述：Test CLI_COMMAND_WITH_BUF with INT_ARRAY
 *
 * 本用例演示如何使用 CLI_COMMAND_WITH_BUF 宏，为命令指定独立的
 * 参数解析缓冲区，而非使用默认的 g_cli_cmd_buf 共享内存。
 * 适用于结构体较大、或需要在 handler 中长期持有 arg_struct 指针的场景。
 *
 * 选项列表：
 *   -v, --verbose    BOOL        Enable verbose
 *   -n, --nums       INT_ARRAY   Number list (max 16)
 *
 * 使用示例：
 *   tw -v -n 10 20 30
 *
 * 预期输出（颜色前缀已省略）：
 *   WITH_BUF test executed!
 *     verbose = true
 *     nums = 10 20 30
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct tw_args {
	bool verbose;
	int *nums;
	size_t nums_count;
};

/* 用户自定义的独立参数缓冲区 */
static char tw_buf[1024];

static int tw_handler(void *_args)
{
	struct tw_args *args = _args;
	cli_printk("WITH_BUF test executed!\n");
	if (args->verbose)
		cli_printk("  verbose = true\n");
	if (args->nums && args->nums_count > 0) {
		cli_printk("  nums = ");
		for (size_t i = 0; i < args->nums_count; i++)
			cli_printk(KERN_NOTICE "%d ", args->nums[i]);
		cli_printk(KERN_NOTICE "\n");
	}
	return 0;
}

CLI_COMMAND_WITH_BUF(tw, "tw", "Test CLI_COMMAND_WITH_BUF with INT_ARRAY",
		     tw_handler, (struct tw_args *)0, tw_buf, sizeof(tw_buf),
		     OPTION('v', "verbose", BOOL, "Enable verbose",
			    struct tw_args, verbose),
		     OPTION('n', "nums", INT_ARRAY, "Number list",
			    struct tw_args, nums, 16, NULL),
		     END_OPTIONS);
