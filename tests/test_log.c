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

#include "cmd_dispose.h"
#include "cli_io.h"

struct log_args {
	char *file;
	int level;
	bool verbose;
	int *tags;
	size_t tags_count;
};

static int log_handler(void *_args)
{
	struct log_args *args = _args;
	cli_printk("LOG command executed!\n");
	if (args->file)
		cli_printk("  file  = %s\n", args->file);
	cli_printk("  level = %d\n", args->level);
	if (args->verbose)
		cli_printk("  verbose = true\n");
	if (args->tags && args->tags_count > 0) {
		cli_printk("  tags  = ");
		for (size_t i = 0; i < args->tags_count; i++)
			cli_printk(KERN_NOTICE "%d ", args->tags[i]);
		cli_printk(KERN_NOTICE "\n");
	}
	return 0;
}

CLI_COMMAND(log, "log", "Configure logger", log_handler, (struct log_args *)0,
	    OPTION('f', "file", STRING, "Log file path", struct log_args, file,
		   true),
	    OPTION('l', "level", INT, "Log level", struct log_args, level, true),
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct log_args,
		   verbose),
	    OPTION('t', "tags", INT_ARRAY, "Tag list", struct log_args, tags, 8,
		   "!verbose"),
	    END_OPTIONS);
