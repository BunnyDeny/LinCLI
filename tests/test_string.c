/**
 * @file test_string.c
 * @brief CLI 框架 STRING 类型选项测试用例。
 *
 * 注册命令 ts，用于验证字符串选项 (-m / --msg) 的解析：
 *   - 用户输入的字符串值会被直接保存为 char* 指针
 *   - 对应结构体字段指向 argv 中切分后的字符串
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct string_args {
	char *msg;
};

static int string_handler(void *_args)
{
	struct string_args *args = _args;
	pr_notice("STRING test executed!\n");
	if (args->msg)
		pr_notice("  msg = %s\n", args->msg);
	return 0;
}

CLI_COMMAND(ts, "ts", "Test STRING option", string_handler,
	    (struct string_args *)0,
	    OPTION('m', "msg", STRING, "Message text", struct string_args, msg),
	    END_OPTIONS);
