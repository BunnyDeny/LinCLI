/**
 * @file test_callback.c
 * @brief CLI 框架 CALLBACK 类型选项测试用例。
 *
 * 注册命令 tc，用于验证 CALLBACK 类型选项 (-c / --cfg) 的解析：
 *   - CALLBACK 在解析阶段与 STRING 行为一致：接收一个字符串参数
 *   - 区别仅在于语义层面，表明该值将由 validator/handler 自行解释处理
 *   - 适用于框架未内置但用户需要自定义解析逻辑的场景
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
