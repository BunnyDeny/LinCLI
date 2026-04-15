/**
 * @file test_double.c
 * @brief CLI 框架 DOUBLE 类型选项测试用例。
 *
 * 注册命令：td
 * 命令描述：Test DOUBLE option
 *
 * 选项列表：
 *   -f, --factor    DOUBLE    Float value
 *
 * 使用示例：
 *   td -f 3.14
 *
 * 预期输出（颜色前缀已省略）：
 *   DOUBLE test executed!
 *     factor = 3.140000
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
