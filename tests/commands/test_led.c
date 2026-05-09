/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

#include "cli_config.h"

#if CLI_ENABLE_DEMO_LED
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

	if (args->on) {
		cli_printk("LED ON, brightness=%d\r\n", args->brightness);
	}
	if (args->off) {
		cli_printk("LED OFF\r\n");
	}
	return 0;
}

CLI_COMMAND(led, "led", "Control LED",
	    USAGE("led --on [-b <brightness>]", "led --off"),
	    led_handler, (struct led_args *)0,
	    OPTION(0, "off", BOOL, "Turn LED off", struct led_args, off, 0,
		   NULL, "brightness", false),
	    OPTION(0, "on", BOOL, "Turn LED on", struct led_args, on, 0,
		   "brightness", "off", false),
	    OPTION('b', "brightness", INT, "Brightness 0-100", struct led_args,
		   brightness, 0, "on", NULL, false),
	    END_OPTIONS);

#endif /* CLI_ENABLE_DEMO_LED */
