/**
 * @file test_bool.c
 * @brief CLI 框架 BOOL 类型选项测试用例。
 *
 * 注册命令：tb
 * 命令描述：Test BOOL option
 *
 * 选项列表：
 *   -v, --verbose    BOOL    Enable verbose
 *
 * 使用示例：
 *   tb -v
 *
 * 预期输出（颜色前缀已省略）：
 *   BOOL test executed!
 *     verbose = true
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct bool_args {
	bool verbose;
};

static int bool_handler(void *_args)
{
	struct bool_args *args = _args;
	cli_printk("BOOL test executed!\n");
	if (args->verbose)
		cli_printk("  verbose = true\n");
	return 0;
}

CLI_COMMAND(tb, "tb", "Test BOOL option", bool_handler, (struct bool_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct bool_args,
		   verbose),
	    END_OPTIONS);
