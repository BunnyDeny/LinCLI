/**
 * @file test_conflicts.c
 * @brief CLI 框架 conflicts（互斥）选项测试用例。
 *
 * 注册命令：tcf
 * 命令描述：Test INT_ARRAY option with conflicts (!verbose)
 *
 * 选项列表：
 *   -v, --verbose    BOOL        Enable verbose
 *   -n, --nums       INT_ARRAY   Number list (max 8, conflicts with verbose)
 *
 * 使用示例：
 *   tcf -n 1 2 3     -> 正常
 *   tcf -v           -> 正常
 *   tcf -v -n 1 2 3  -> 报错：选项 -n/--nums 与 verbose 互斥，不能同时使用
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct conflicts_args {
	bool verbose;
	int *nums;
	size_t nums_count;
};

static int conflicts_handler(void *_args)
{
	struct conflicts_args *args = _args;
	pr_notice("CONFLICTS test executed!\n");
	if (args->verbose)
		pr_notice("  verbose = true\n");
	if (args->nums && args->nums_count > 0) {
		pr_notice("  nums = ");
		for (size_t i = 0; i < args->nums_count; i++)
			cli_printk(KERN_NOTICE "%d ", args->nums[i]);
		cli_printk(KERN_NOTICE "\n");
	}
	return 0;
}

CLI_COMMAND(tcf, "tcf", "Test INT_ARRAY option with conflicts", conflicts_handler,
	    (struct conflicts_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct conflicts_args,
		   verbose),
	    OPTION('n', "nums", INT_ARRAY, "Number list", struct conflicts_args, nums,
		   8, "!verbose"),
	    END_OPTIONS);
