/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 */

/**
 * @file test_raw_cmd.c
 * @brief Raw argument command (argc/argv style) test.
 *
 * 注册命令：cp
 * 命令描述：Raw argument command test
 *
 * 使用示例：
 *   cp src.txt dst.txt
 *
 * 预期输出：
 *   argc=3
 *   argv[0]=cp
 *   argv[1]=src.txt
 *   argv[2]=dst.txt
 */

#include "cli_config.h"

#if CLI_ENABLE_DEMO_RAW_CMD
#include "cmd_dispose.h"
#include "cli_io.h"

static int cp_handler(char **argv, int argc)
{
	cli_printk("argc=%d\r\n", argc);
	for (int i = 0; i < argc; i++)
		cli_printk("argv[%d]=%s\r\n", i, argv[i]);
	return 0;
}

CLI_RAW_COMMAND(cp_cmd, "cp", "Raw argument command test", cp_handler,
		"file1.txt", "file2.txt", "file3.txt");

#endif /* CLI_ENABLE_DEMO_RAW_CMD */
