/**
 * @file test_double.c
 * @brief CLI 框架 DOUBLE 类型选项测试用例。
 *
 * 注册命令 td，用于验证浮点选项 (-f / --factor) 的解析：
 *   - 用户输入的字符串值通过 atof() 转换为 double
 *   - 结果写入结构体对应字段
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct double_args {
	double factor;
};

static int double_handler(void *_args)
{
	struct double_args *args = _args;
	pr_notice("DOUBLE test executed!\n");
	pr_notice("  factor = %f\n", args->factor);
	return 0;
}

CLI_COMMAND(td, "td", "Test DOUBLE option", double_handler,
	    (struct double_args *)0,
	    OPTION('f', "factor", DOUBLE, "Float value", struct double_args, factor),
	    END_OPTIONS);
