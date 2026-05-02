/*
 * LinCLI - Quote protection test case.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/**
 * @file test_quotes.c
 * @brief 引号保护机制测试用例。
 *
 * 注册命令：tquote
 * 命令描述：Test quote protection
 *
 * 使用示例：
 *   tquote -m 'hello world'
 *   tquote -m "hello && world"
 *   tquote -m ''
 *
 * 预期输出（颜色前缀已省略）：
 *   msg=[hello world]
 *   msg=[hello && world]
 *   msg=[]
 */

#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct quote_args {
	char *msg;
};

static int quote_handler(void *_args)
{
	struct quote_args *args = _args;
	cli_printk("msg=[%s]\r\n", args->msg ? args->msg : "");
	return 0;
}

CLI_COMMAND(tquote, "tquote", "Test quote protection",
	    USAGE("tquote -m <msg>"), quote_handler, (struct quote_args *)0,
	    OPTION('m', "msg", STRING, "Message text", struct quote_args, msg,
		   0, NULL, NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
