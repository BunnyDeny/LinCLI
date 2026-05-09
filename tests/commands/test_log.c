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

#if CLI_ENABLE_DEMO_LOG
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

CLI_COMMAND(log, "log", "Configure logger",
	    USAGE("log -f <file> -l <level> [-v] [-t <tags...>]"),
	    log_handler, (struct log_args *)0,
	    OPTION('f', "file", STRING, "Log file path", struct log_args, file,
		   0, NULL, NULL, true),
	    OPTION('l', "level", INT, "Log level", struct log_args, level, 0,
		   NULL, NULL, true),
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct log_args,
		   verbose, 0, NULL, NULL, false),
	    OPTION('t', "tags", INT_ARRAY, "Tag list", struct log_args, tags, 8,
		   NULL, "verbose", false),
	    OPTION('c', "verbose1", BOOL, "Enable verbose", struct log_args,
		   verbose, 0, NULL, NULL, false),
	    OPTION('a', "verbose12", BOOL, "Enable verbose", struct log_args,
		   verbose, 0, NULL, NULL, false),
	    OPTION('b', "verbose13", BOOL, "Enable verbose", struct log_args,
		   verbose, 0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_LOG */
