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
 * @file test_led.c
 * @brief LED 控制命令测试用例。
 *
 * 注册命令：led
 * 命令描述：Control LED
 *
 * 选项列表：
 *   --on             BOOL    Turn LED on
 *   --off            BOOL    Turn LED off
 *   -b, --brightness INT     Brightness 0-100 (depends on --on)
 *
 * 使用示例：
 *   led --on -b 80
 *   led --off
 *   led --help
 */

#ifdef CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct led_args {
	bool on;
	bool off;
	int brightness;
};

static int led_handler(void *_args)
{
	struct led_args *args = _args;

	if (!args->on && !args->off) {
		pr_err("please specify --on or --off\r\n");
		return -1;
	}
	if (args->on) {
		cli_printk("LED ON");
		if (args->brightness > 0)
			cli_printk(", brightness=%d", args->brightness);
		cli_printk("\r\n");
	}
	if (args->off) {
		cli_printk("LED OFF\r\n");
	}
	return 0;
}

CLI_COMMAND(led, "led", "Control LED", led_handler, (struct led_args *)0,
	    OPTION(0, "off", BOOL, "Turn LED off", struct led_args, off, 0,
		   NULL, "brightness", false),
	    OPTION(0, "on", BOOL, "Turn LED on", struct led_args, on, 0,
		   "brightness", "off", false),
	    OPTION('b', "brightness", INT, "Brightness 0-100", struct led_args,
		   brightness, 0, "on", NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_TESTS */
