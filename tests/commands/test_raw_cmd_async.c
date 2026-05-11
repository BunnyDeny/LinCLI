/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 */

/**
 * @file test_raw_cmd_async.c
 * @brief Async raw argument command test.
 *
 * 注册命令：mv
 * 命令描述：Async raw argument command test
 *
 * 使用示例：
 *   mv old.txt new.txt
 *
 * 预期输出：
 *   [entry] argc=3
 *   [task] argc=3
 *   [exit] argc=3
 */

#include "cli_config.h"

#if CLI_ENABLE_DEMO_RAW_CMD_ASYNC
#include "cmd_dispose.h"
#include "cli_io.h"

static int mv_tick;

static void mv_entry(char **argv, int argc)
{
	cli_printk("[entry] argc=%d\r\n", argc);
	for (int i = 0; i < argc; i++)
		cli_printk("  argv[%d]=%s\r\n", i, argv[i]);
	mv_tick = 0;
}

static int mv_task(char **argv, int argc)
{
	cli_printk("[task] argc=%d tick=%d\r\n", argc, mv_tick);
	mv_tick++;
	if (mv_tick >= 3)
		return 0;
	return CLI_CONTINUE;
}

static void mv_exit(char **argv, int argc)
{
	cli_printk("[exit] argc=%d\r\n", argc);
}

CLI_RAW_COMMAND_ASYNC(mv_cmd, "mv", "Async raw argument command test",
				USAGE("mv <old> <new>"),
				mv_entry, mv_task, mv_exit,
				"doc1.txt", "doc2.txt");

#endif /* CLI_ENABLE_DEMO_RAW_CMD_ASYNC */
