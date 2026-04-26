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
 * @file test_key_input.c
 * @brief CLI 框架按键输入交互测试用例。
 *
 * 注册命令：key
 * 命令描述：Test key
 *
 * 交互说明：
 *   进入测试后，程序将实时捕获并回显用户输入的每一个字符及其对应的 ASCII 码。
 *   该测试用于验证 CLI 的底层输入流（Input Stream）捕获和缓冲区处理能力。
 *
 * 退出方式：
 *   - Ctrl+D (ASCII 4):  检测到该组合键后，程序正常退出（Normal Exit）。
 *   - Ctrl+W (ASCII 23): 检测到该组合键后，程序异常终止（Aborting）。
 *
 * 使用示例：
 *   key
 *
 * 预期输出（动态刷新）：
 *    lin@linCli> key 
 *     get a, ascci  97       
    
 *   [INFO] Key ctrl+d detected, exiting
 */
#include "cli_config.h"

#ifdef CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

static int key_handler(void *_args)
{
	int status, size;
	char ch;
	reset_cli_in_push_lock();
	pr_info("test_key_interaction, press ctrl+d or ctrl+w to exit.\r\n");
	while (1) {
		size = cli_get_in_size();
		if (size) {
			status = cli_in_pop((_u8 *)&ch, 1);
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
	}
}

CLI_COMMAND_NO_STRUCT(key, "key", "Test key", key_handler);

#endif /* CLI_ENABLE_TESTS */
