/**
 * @file test_int.c
 * @brief CLI 框架 INT 类型选项测试用例。
 *
 * 注册命令 ti，用于验证整数选项 (-n / --num) 的解析：
 *   - 用户输入的字符串值通过 atoi() 转换为 int
 *   - 结果写入结构体对应字段
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
