/**
 * @file test_bool.c
 * @brief CLI 框架 BOOL 类型选项测试用例。
 *
 * 注册命令 tb，用于验证无参数开关选项 (-v / --verbose) 的解析：
 *   - 选项存在时，对应结构体字段被置为 true
 *   - 选项不存在时，字段保持 false（框架会在解析前清零结构体）
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct bool_args {
	bool verbose;
};

static int bool_handler(void *_args)
{
	struct bool_args *args = _args;
	pr_notice("BOOL test executed!\n");
	if (args->verbose)
		pr_notice("  verbose = true\n");
	return 0;
}

CLI_COMMAND(tb, "tb", "Test BOOL option", bool_handler,
	    (struct bool_args *)0, OPTION('v', "verbose", BOOL, "Enable verbose",
					  struct bool_args, verbose),
	    END_OPTIONS);
