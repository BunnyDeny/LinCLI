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

#include "cli_io.h"
#include "cli_critical.h"
#include "cmd_dispose.h"
#include "cli_cmd_line.h"
#include "cli_completion.h"
#include "cli_mpool.h"
#include "cli_vsnprintf.h"
#include <stdarg.h>
#include <string.h>
//#include <unistd.h>

char log_level[3] = "8";

_u8 cli_in_push_lock = 1;

struct cli_io _cli_io = {
	.in_ref = 0,
	.out_ref = 0,
};

void cli_io_init(void)
{
	kfifo_init(&_cli_io.in, (uint8_t *)_cli_io.in_buf, CLI_IO_SIZE);
	kfifo_init(&_cli_io.out, (uint8_t *)_cli_io.out_buf, CLI_IO_SIZE);
	_cli_io.in_ref = 1;
	_cli_io.out_ref = 1;
}

//移植的时候实现
__attribute__((weak)) void cli_putc(char ch)
{
}

int cli_in_push(_u8 *data, int size)
{
	uint32_t ret;
	if (cli_in_push_lock || _cli_io.in_ref == 0) {
		return CLI_ERR_FIFO_FULL;
	}
	ret = kfifo_put(&_cli_io.in, (const uint8_t *)data, (uint32_t)size);
	if (ret < (uint32_t)size) {
		return CLI_ERR_FIFO_FULL;
	}
	return CLI_OK;
}

int cli_out_push(_u8 *data, int size)
{
	uint32_t ret;
	if (_cli_io.out_ref == 0) {
		return CLI_ERR_INVAL;
	}
	cli_enter_critical();
	ret = kfifo_put(&_cli_io.out, (const uint8_t *)data, (uint32_t)size);
	cli_exit_critical();
	if (ret < (uint32_t)size) {
		return CLI_ERR_FIFO_FULL;
	}
	return CLI_OK;
}

int cli_in_pop(_u8 *data, int size)
{
	uint32_t ret;
	if (_cli_io.in_ref == 0) {
		return CLI_ERR_INVAL;
	}
	ret = kfifo_get(&_cli_io.in, (uint8_t *)data, (uint32_t)size);
	return (int)ret;
}

int cli_out_pop(_u8 *data, int size)
{
	uint32_t ret;
	if (_cli_io.out_ref == 0) {
		return CLI_ERR_INVAL;
	}
	cli_enter_critical();
	ret = kfifo_get(&_cli_io.out, (uint8_t *)data, (uint32_t)size);
	cli_exit_critical();
	return (int)ret;
}

int cli_get_in_size(void)
{
	return (int)kfifo_len(&_cli_io.in);
}

int cli_get_out_size(void)
{
	int size;
	cli_enter_critical();
	size = (int)kfifo_len(&_cli_io.out);
	cli_exit_critical();
	return size;
}

int cli_out_sync(void)
{
	while (cli_get_out_size() > 0) {
		char ch;
		int status = cli_out_pop((_u8 *)&ch, 1);
		if (status < 0) {
			return status;
		}
		if (status == 0) {
			return CLI_ERR_FIFO_EMPTY;
		}
		cli_putc(ch);
	}
	return 0;
}

static const char *prefix_table[] = {
	COLOR_BOLD COLOR_RED "[E] ",
	COLOR_MAGENTA "[A] ",
	COLOR_RAINBOW_2 "[C] ",
	COLOR_RED "[E] ",
	COLOR_YELLOW "[W] ",
	COLOR_BOLD COLOR_GREEN " ",
	COLOR_BLUE "[I] " COLOR_NONE,
	COLOR_RAINBOW_4 "[D] ",
	"",
};

int all_printk(const char *fmt, ...)
{
	int status;
	char buf[CLI_PRINTK_BUF_SIZE];
	va_list args;
	va_start(args, fmt);
	int len = cli_vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	status = cli_out_push((_u8 *)buf, len);
	if (status < 0)
		return status;
	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;
	return 0;
}

int cli_in_clear(void)
{
	if (_cli_io.in_ref == 0) {
		return CLI_ERR_INVAL;
	}
	kfifo_reset(&_cli_io.in);
	return 0;
}

void set_cli_in_push_lock(void)
{
	cli_in_push_lock = 1;
}

void reset_cli_in_push_lock(void)
{
	cli_in_push_lock = 0;
}

/* ============================================================
 *  level 命令：控制日志过滤级别
 * ============================================================ */

struct level_args {
	bool emerg;
	bool alert;
	bool crit;
	bool err;
	bool warning;
	bool notice;
	bool info;
	bool debug;
	bool default_lvl;
	bool cont;
};

static int level_handler(void *_args)
{
	struct level_args *args = _args;
	if (args->emerg)
		strcpy(log_level, KERN_EMERG);
	else if (args->alert)
		strcpy(log_level, KERN_ALERT);
	else if (args->crit)
		strcpy(log_level, KERN_CRIT);
	else if (args->err)
		strcpy(log_level, KERN_ERR);
	else if (args->warning)
		strcpy(log_level, KERN_WARNING);
	else if (args->notice)
		strcpy(log_level, KERN_NOTICE);
	else if (args->info)
		strcpy(log_level, KERN_INFO);
	else if (args->debug)
		strcpy(log_level, KERN_DEBUG);
	else
		strcpy(log_level, "8");
	return 0;
}

CLI_COMMAND(level, "level", "Log level",
	    USAGE("level [options]"),
	    level_handler, (struct level_args *)0,
	    OPTION(0, "emerg", BOOL, "",
		   struct level_args, emerg, 0, NULL, NULL, false),
	    OPTION(0, "alert", BOOL, "",
		   struct level_args, alert, 0, NULL, NULL, false),
	    OPTION(0, "crit", BOOL, "",
		   struct level_args, crit, 0, NULL, NULL, false),
	    OPTION(0, "err", BOOL, "",
		   struct level_args, err, 0, NULL, NULL, false),
	    OPTION(0, "warning", BOOL, "",
		   struct level_args, warning, 0, NULL, NULL, false),
	    OPTION(0, "notice", BOOL, "",
		   struct level_args, notice, 0, NULL, NULL, false),
	    OPTION(0, "info", BOOL, "",
		   struct level_args, info, 0, NULL, NULL, false),
	    OPTION(0, "debug", BOOL, "",
		   struct level_args, debug, 0, NULL, NULL, false),
	    END_OPTIONS);

/* ============================================================
 *  cli_printk 辅助函数（从 cli_io.c 迁移而来）
 * ============================================================ */

static char buffer[CLI_PRINTK_BUF_SIZE];

static const char *prefix_gen(const char *level)
{
	char lv = level[0];
	if (lv >= '0' && lv <= '7')
		return prefix_table[lv - '0'];
	return prefix_table[8];
}

static inline int is_kern_level(char c)
{
	return (c >= '0' && c <= '7');
}

/* ============================================================
 *  cli_printk（从 cli_io.c 迁移至此，直接访问 cmd_line 状态）
 * ============================================================ */

extern int scheduler_is_in_get_char(void);

static bool printk_should_drop(const char *pre)
{
	if (pre[0] != '8' && pre[0] >= '0' && pre[0] <= '7') {
		if (pre[0] > log_level[0])
			return true;
	}
	if (!is_kern_level(pre[0]) && strcmp("8", log_level))
		return true;
	return false;
}

static int printk_format_and_send(const char *pre_str, int raw_len)
{
	if (raw_len <= 0)
		return 0;

	int pre_len = strlen(pre_str);
	int suffix_len = strlen(COLOR_NONE);

	/* 如果 buffer[0] 是内核级别字符，跳过它 */
	const char *content = buffer;
	int content_len = raw_len;
	if (is_kern_level(buffer[0])) {
		content++;
		content_len--;
	}

	if (content_len <= 0)
		return 0;

	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;

	int status;

	if (pre_len > 0) {
		status = cli_out_push((_u8 *)pre_str, pre_len);
		if (status < 0)
			return status;
	}

	status = cli_out_push((_u8 *)content, content_len);
	if (status < 0)
		return status;

	if (suffix_len > 0) {
		status = cli_out_push((_u8 *)COLOR_NONE, suffix_len);
		if (status < 0)
			return status;
	}

	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;

	return 0;
}

int cli_printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = cli_vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	char pre[2] = { buffer[0], '\0' };
	if (printk_should_drop(pre))
		return 0;

	int in_interactive = scheduler_is_in_get_char();
	if (in_interactive)
		cli_out_push((_u8 *)"\r\033[K", 4);

	const char *_pre = prefix_gen(pre);
	int status = printk_format_and_send(_pre, len);
	if (status < 0)
		return status;

	if (in_interactive) {
		if (candidate_ctx.active)
			candidate_redraw();
		else
			cmd_line_redraw();
	}
	return len;
}

/* ============================================================
 *  内存池占用情况打印（分配失败时自动调用，不申请内存）
 * ============================================================ */

__attribute__((weak)) void cli_mpool_dump_usage(void)
{
	const char *owners[CLI_MPOOL_COUNT];
	int used_count = 0;

	cli_mpool_get_usage(owners, &used_count);

	pr_crit("mpool OOM %d/%d\r\n", used_count,
		CLI_MPOOL_COUNT);
	for (int i = 0; i < used_count; i++) {
		pr_crit("[%d] %s\r\n", i,
			owners[i] ? owners[i] : "?");
	}
}
