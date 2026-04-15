/**
 * @file test_int.c
 * @brief CLI 框架 INT 类型选项测试用例。
 *
 * 注册命令：ti
 * 命令描述：Test INT option
 *
 * 选项列表：
 *   -n, --num    INT    Integer value
 *
 * 使用示例：
 *   ti -n 42
 *
 * 预期输出（颜色前缀已省略）：
 *   INT test executed!
 *     num = 42
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct int_args {
	int num;
};

static int int_handler(void *_args)
{
	struct int_args *args = _args;
	pr_notice("INT test executed!\n");
	pr_notice("  num = %d\n", args->num);
	return 0;
}

CLI_COMMAND(ti, "ti", "Test INT option", int_handler,
	    (struct int_args *)0,
	    OPTION('n', "num", INT, "Integer value", struct int_args, num),
	    END_OPTIONS);
