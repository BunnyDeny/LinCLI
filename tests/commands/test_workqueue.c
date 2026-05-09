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
 * @file test_workqueue.c
 * @brief Workqueue 集成测试用例。
 *
 * 注册命令：wqtest
 * 命令描述：Test workqueue scheduling
 *
 * 无选项。
 *
 * 使用示例：
 *   wqtest
 *
 * 预期输出（在 tick 轮询中异步打印）：
 *   [WQ] immediate work executed
 *   [WQ] delayed work executed (after 3 ticks)
 */

#include "cli_config.h"

#if CLI_ENABLE_DEMO_WORKQUEUE
#include "cmd_dispose.h"
#include "cli_io.h"
#include "workqueue.h"

static struct work_struct wq_immediate;
static struct delayed_work wq_delayed;
static int wq_inited = 0;

extern volatile unsigned long jiffies;

static void wq_immediate_handler(struct work_struct *work)
{
	(void)work;
	cli_printk("[WQ] immediate work executed at tick %lu\r\n", jiffies);
}

static void wq_delayed_handler(struct work_struct *work)
{
	(void)work;
	cli_printk("[WQ] delayed work executed at tick %lu (after 3 ticks)\r\n", jiffies);
}

static int wqtest_handler(void *_args)
{
	(void)_args;

	if (!wq_inited) {
		INIT_WORK(&wq_immediate, wq_immediate_handler);
		INIT_DELAYED_WORK(&wq_delayed, wq_delayed_handler);
		wq_inited = 1;
	}

	cli_printk("Scheduling workqueue test jobs...\r\n");
	schedule_work(&wq_immediate);
	schedule_delayed_work(&wq_delayed, 3);
	cli_printk("Jobs scheduled at tick %lu, watch tick loop output\r\n", jiffies);
	return 0;
}

CLI_COMMAND_NO_STRUCT(wqtest, "wqtest", "Test workqueue scheduling",
		      wqtest_handler);

#endif /* CLI_ENABLE_DEMO_WORKQUEUE */
