/*
 * LinCLI - Environment variable test case.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"
#include "cli_env.h"

/* ============================================================
 *  测试环境变量注册
 * ============================================================ */

CLI_ENV(GREETING, "Hello LinCLI");
CLI_ENV(PROJECT_NAME, "LinCLI-Framework");
CLI_ENV(EMPTY_VAR, "");

/* 特殊字符测试 */
CLI_ENV(TEST_DASH, "hello -v");
CLI_ENV(TEST_CHAIN, "echo hello && echo world");
CLI_ENV(TEST_MIX, "hello-world");

/* 纯整数名字，应被运行时忽略 */
CLI_ENV(123, "should_be_ignored");

/* ============================================================
 *  techo 命令：用于验证环境变量替换效果
 *
 *  示例：
 *    techo --msg $GREETING
 *    techo --msg $PROJECT_NAME
 *    techo --msg $0        (按 ID 引用 GREETING)
 * ============================================================ */

struct techo_args {
	const char *msg;
};

static int techo_handler(void *_args)
{
	struct techo_args *args = _args;
	cli_printk("%s\r\n", args->msg ? args->msg : "");
	return 0;
}

CLI_COMMAND(techo, "techo", "Test echo with env substitution",
	    USAGE("techo --msg <message>"), techo_handler,
	    (struct techo_args *)0,
	    OPTION(0, "msg", STRING, "Message to echo", struct techo_args, msg,
		   0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
