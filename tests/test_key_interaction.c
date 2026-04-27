/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file test_key_interaction.c
 * @brief CLI 框架按键输入交互测试用例（异步非阻塞版本）。
 *
 * 注册命令：key
 * 命令描述：Test key input interaction (async)
 *
 * 交互说明：
 *   进入测试后，程序将在每次 scheduler 轮询时捕获并回显用户输入的字符
 *   及其对应的 ASCII 码。命令不独占 CPU，执行期间 scheduler_task() 会正常
 *   返回，用户的 while(1) 业务逻辑不会被阻塞。
 *
 * 退出方式：
 *   - Ctrl+D (ASCII 4):  正常退出（Normal Exit）。
 *   - Ctrl+W (ASCII 23): 异常终止（Aborting）。
 *
 * 使用示例：
 *   key
 *
 * 预期输出（动态刷新）：
 *    lin@linCli> key
 *   [INFO] test_key_interaction, press ctrl+d or ctrl+w to exit.
 *     get a, ascii  97
 *
 *   [INFO] Key ctrl+d detected, exiting
 */
#include "cli_config.h"

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct key_args {
	/* 本命令无需命令行选项参数 */
};

static void key_entry(void *_args)
{
	(void)_args;
	/* 解锁输入缓冲区，允许在命令执行期间接收按键 */
	reset_cli_in_push_lock();
	pr_info("test_key_interaction, press ctrl+d or ctrl+w to exit.\r\n");
}

static int key_task(void *_args)
{
	(void)_args;
	int size = cli_get_in_size();

	if (size > 0) {
		char ch;
		int status = cli_in_pop((_u8 *)&ch, 1);
		if (status < 0) {
			return status;
		}

		if (ch == (char)4) {
			cli_printk("\r\n");
			pr_info("Key ctrl+d detected, exiting\r\n");
			return 0;
		}

		if (ch == (char)23) {
			cli_printk("\r\n");
			pr_err("Key ctrl+w detected, aborting.\n");
			return -1;
		}

		cli_printk("\r get %c, ascii %3d       ",
			   (ch >= 32 && ch <= 126) ? ch : ' ', (int)ch);
	}

	/* 没有数据或只处理了一个字符，继续等待下次轮询 */
	return CLI_CONTINUE;
}

static void key_exit(void *_args)
{
	(void)_args;
	/* 输入缓冲区锁由框架在回到 scheduler_get_char 时自动管理，
	 * 此处无需额外操作。
	 */
}

CLI_COMMAND_ASYNC(key, "key", "Test key input interaction (async)",
		  key_entry, key_task, key_exit,
		  (struct key_args *)0,
		  END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
