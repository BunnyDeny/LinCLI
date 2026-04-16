/**
 * @file test_required.c
 * @brief CLI 框架 OPTION_7（基础类型 + required）测试用例。
 *
 * 注册命令：tr
 * 命令描述：Test required option
 *
 * 选项列表：
 *   -f, --file    STRING [必需]    Input file path
 *
 * 使用示例：
 *   tr -f /tmp/data.txt    -> 正常
 *   tr                     -> 报错：缺少必需选项
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct required_args {
	char *file;
};

static int required_handler(void *_args)
{
	struct required_args *args = _args;
	cli_printk("REQUIRED test executed!\n");
	if (args->file)
		cli_printk("  file = %s\n", args->file);
	return 0;
}

CLI_COMMAND(tr, "tr", "Test required option", required_handler,
	    (struct required_args *)0,
	    OPTION('f', "file", STRING, "Input file path", struct required_args,
		   file, true),
	    END_OPTIONS);
