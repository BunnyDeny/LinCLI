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
 * @file test_log.c
 * @brief 综合复杂命令测试用例（演示 OPTION_9 + 互斥 + 必需选项）。
 *
 * 注册命令：log
 * 命令描述：Configure logger
 *
 * 选项列表：
 *   -f, --file    STRING [必需]    Log file path
 *   -l, --level   INT    [必需]    Log level
 *   -v, --verbose BOOL             Enable verbose
 *   -t, --tags    INT_ARRAY [互斥:verbose] Tag list (max 8)
 *
 * 使用示例：
 *   log -f /tmp/app.log -l 3 -t 1 2 3
 *
 * 预期输出（颜色前缀已省略）：
 *   LOG command executed!
 *     file  = /tmp/app.log
 *     level = 3
 *     tags  = 1 2 3
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct log_args {
	char *file;
	int level;
	bool verbose;
	bool verbose2;
	bool verbose3;
	int *tags;
	size_t tags_count;
};

static int log_handler(void *_args)
{
	struct log_args *args = _args;
	cli_printk("LOG command executed!\r\n");
	if (args->file)
		cli_printk("  file  = %s\r\n", args->file);
	cli_printk("  level = %d\r\n", args->level);
	if (args->verbose)
		cli_printk("  verbose = true\r\n");
	if (args->tags && args->tags_count > 0) {
		cli_printk("  tags  = ");
		for (size_t i = 0; i < args->tags_count; i++)
			cli_printk(KERN_NOTICE "%d ", args->tags[i]);
		cli_printk(KERN_NOTICE "\r\n");
	}
	return 0;
}

CLI_COMMAND(log, "log", "Configure logger", log_handler, (struct log_args *)0,
	    OPTION('f', "file", STRING, "Log file path", struct log_args, file,
		   0, NULL, NULL, true),
	    OPTION('l', "level", INT, "Log level", struct log_args, level, 0,
		   NULL, NULL, true),
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct log_args,
		   verbose, 0, NULL, NULL, false),
	    OPTION('t', "tags", INT_ARRAY, "Tag list", struct log_args, tags, 8,
		   NULL, "verbose", false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
