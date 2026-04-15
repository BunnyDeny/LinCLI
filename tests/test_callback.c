/**
 * @file test_callback.c
 * @brief CLI 框架 CALLBACK 类型选项测试用例。
 *
 * 注册命令：tc
 * 命令描述：Test CALLBACK option
 *
 * 选项列表：
 *   -c, --cfg    CALLBACK    Raw config string
 *
 * 使用示例：
 *   tc -c foo
 *
 * 预期输出（颜色前缀已省略）：
 *   CALLBACK test executed!
 *     custom callback triggered with: foo
 */

#include "cmd_dispose.h"
#include "cli_io.h"

struct cb_args {
	const char *raw;
};

static int callback_handler(void *_args)
{
	struct cb_args *args = _args;
	pr_notice("CALLBACK test executed!\n");
	pr_notice("  custom callback triggered with: %s\n",
		  args->raw ? args->raw : "(null)");
	return 0;
}

CLI_COMMAND(tc, "tc", "Test CALLBACK option", callback_handler,
	    (struct cb_args *)0,
	    OPTION('c', "cfg", CALLBACK, "Raw config string", struct cb_args, raw),
	    END_OPTIONS);
