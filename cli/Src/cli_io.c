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

#include "cli_io.h"
#include "cli_critical.h"
#include "cmd_dispose.h"
#include "cli_cmd_line.h"
#include "cli_completion.h"
#include "cli_mpool.h"
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
	vectorInit(&_cli_io.in, (_u8 *)_cli_io.in_buf, CLI_IO_SIZE);
	vectorInit(&_cli_io.out, (_u8 *)_cli_io.out_buf, CLI_IO_SIZE);
	_cli_io.in_ref = 1;
	_cli_io.out_ref = 1;
}

//移植的时候实现
__attribute__((weak)) void cli_putc(char ch)
{
}

static int _cli_io_push(struct vector *v, _u8 *data, int size, _u8 *ref)
{
	bool status;
	if (*ref == 0) {
		return CLI_ERR_INVAL; /*uninited*/
	}
	cli_enter_critical();
	(*ref)++;
	status = push_back(v, data, size);
	(*ref)--;
	cli_exit_critical();
	if (status == false) {
		return CLI_ERR_FIFO_FULL;
	} else {
		return CLI_OK;
	}
}

static int _cli_io_pop(struct vector *v, _u8 *data, int size, _u8 *ref)
{
	if (*ref == 0) {
		return CLI_ERR_INVAL; /*uninited*/
	}
	cli_enter_critical();
	(*ref)++;
	int remain_to_pop = size;
	while (remain_to_pop) {
		_u8 front;
		if (at(v, 0, &front) == false) {
			goto _cli_io_pop_exit;
		}
		int idx = size - remain_to_pop;
		*(data + idx) = front;
		pop_front(v, 1);
		remain_to_pop--;
	}
_cli_io_pop_exit:
	(*ref)--;
	cli_exit_critical();
	return CLI_OK;
}

int cli_in_push(_u8 *data, int size)
{
	if (cli_in_push_lock) {
		return CLI_ERR_FIFO_FULL;
	} else {
		return _cli_io_push(&_cli_io.in, data, size, &_cli_io.in_ref);
	}
}

int cli_out_push(_u8 *data, int size)
{
	return _cli_io_push(&_cli_io.out, data, size, &_cli_io.out_ref);
}

int cli_in_pop(_u8 *data, int size)
{
	return _cli_io_pop(&_cli_io.in, data, size, &_cli_io.in_ref);
}

int cli_out_pop(_u8 *data, int size)
{
	return _cli_io_pop(&_cli_io.out, data, size, &_cli_io.out_ref);
}

int cli_get_in_size(void)
{
	int size;
	cli_enter_critical();
	size = _cli_io.in.size;
	cli_exit_critical();
	return size;
}

int cli_get_out_size(void)
{
	int size;
	cli_enter_critical();
	size = _cli_io.out.size;
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
		} else {
			cli_putc(ch);
		}
	}
	return 0;
}

__attribute__((weak)) const char *pre_EMERG_gen(void)
{
	return COLOR_BOLD COLOR_RED "[EMERG] ";
}

__attribute__((weak)) const char *pre_ALERT_gen(void)
{
	return COLOR_MAGENTA "[ALERT] ";
}

__attribute__((weak)) const char *pre_CRIT_gen(void)
{
	return COLOR_RAINBOW_2 "[CRIT] ";
}

__attribute__((weak)) const char *pre_ERR_gen(void)
{
	return COLOR_RED "[ERR] ";
}

__attribute__((weak)) const char *pre_WARNING_gen(void)
{
	return COLOR_YELLOW "[WARNING] ";
}

__attribute__((weak)) const char *pre_NOTICE_gen(void)
{
	return COLOR_BOLD COLOR_GREEN " ";
}

__attribute__((weak)) const char *pre_INFO_gen(void)
{
	return COLOR_BLUE "[INFO] " COLOR_NONE;
}

__attribute__((weak)) const char *pre_DEBUG_gen(void)
{
	return COLOR_RAINBOW_4 "[DEBUG] ";
}

__attribute__((weak)) const char *pre_DEFAULT_gen(void)
{
	return "";
}

__attribute__((weak)) const char *pre_CONT_gen(void)
{
	return COLOR_CYAN "[CONT] ";
}

int all_printk(const char *fmt, ...)
{
	int status;
	char buf[CLI_PRINTK_BUF_SIZE];
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buf, sizeof(buf), fmt, args);
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
	_u8 tmp;
	int status;
	while (_cli_io.in.size > 0) {
		status = cli_in_pop(&tmp, 1);
		if (status < 0) {
			return status;
		}
	}
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

CLI_COMMAND(level, "level", "Set log level filter",
	    USAGE("level [--emerg|--alert|--crit|--err|--warning|--notice|--info|--debug]"),
	    level_handler, (struct level_args *)0,
	    OPTION(0, "emerg", BOOL, "Set log level to EMERG (0)",
		   struct level_args, emerg, 0, NULL, NULL, false),
	    OPTION(0, "alert", BOOL, "Set log level to ALERT (1)",
		   struct level_args, alert, 0, NULL, NULL, false),
	    OPTION(0, "crit", BOOL, "Set log level to CRIT (2)",
		   struct level_args, crit, 0, NULL, NULL, false),
	    OPTION(0, "err", BOOL, "Set log level to ERR (3)",
		   struct level_args, err, 0, NULL, NULL, false),
	    OPTION(0, "warning", BOOL, "Set log level to WARNING (4)",
		   struct level_args, warning, 0, NULL, NULL, false),
	    OPTION(0, "notice", BOOL, "Set log level to NOTICE (5)",
		   struct level_args, notice, 0, NULL, NULL, false),
	    OPTION(0, "info", BOOL, "Set log level to INFO (6)",
		   struct level_args, info, 0, NULL, NULL, false),
	    OPTION(0, "debug", BOOL, "Set log level to DEBUG (7)",
		   struct level_args, debug, 0, NULL, NULL, false),
	    END_OPTIONS);

/* ============================================================
 *  cli_printk 辅助函数（从 cli_io.c 迁移而来）
 * ============================================================ */

static char buffer[CLI_PRINTK_BUF_SIZE];


static const char *prefix_gen(const char *level)
{
	char lv = level[0];
	const char *prefix;
	switch (lv) {
	case '0':
		prefix = pre_EMERG_gen();
		break;
	case '1':
		prefix = pre_ALERT_gen();
		break;
	case '2':
		prefix = pre_CRIT_gen();
		break;
	case '3':
		prefix = pre_ERR_gen();
		break;
	case '4':
		prefix = pre_WARNING_gen();
		break;
	case '5':
		prefix = pre_NOTICE_gen();
		break;
	case '6':
		prefix = pre_INFO_gen();
		break;
	case '7':
		prefix = pre_DEBUG_gen();
		break;
	case 'c':
		prefix = pre_CONT_gen();
		break;
	default:
		prefix = pre_DEFAULT_gen();
		break;
	}
	return prefix;
}

static inline int is_kern_level(char c)
{
	return (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' ||
		c == '5' || c == '6' || c == '7' || c == 'c');
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
	if ((!is_kern_level(pre[0]) || !strcmp(pre, KERN_CONT)) &&
	    strcmp("8", log_level))
		return true;
	return false;
}

static int printk_format_and_send(const char *pre_str, int raw_len)
{
	if (is_kern_level(buffer[0]))
		memmove(buffer, buffer + 1, CLI_PRINTK_BUF_SIZE - 1);
	int pre_len = strlen(pre_str);
	if (raw_len <= 0 || pre_len < 0)
		return 0;
	memmove(buffer + pre_len, buffer, raw_len + 1);
	memcpy(buffer, pre_str, pre_len);
	strcat(buffer, COLOR_NONE);
	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;
	int status = cli_out_push((_u8 *)buffer,
				  pre_len + raw_len + strlen(COLOR_NONE));
	if (status < 0)
		return status;
	if (cli_out_sync())
		return CLI_ERR_IO_SYNC;
	return 0;
}

int cli_printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
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

void cli_mpool_dump_usage(void)
{
	const char *owners[CLI_MPOOL_COUNT];
	int used_count = 0;

	cli_mpool_get_usage(owners, &used_count);

	pr_crit("[mpool] exhausted! %d/%d blocks used\r\n", used_count,
		CLI_MPOOL_COUNT);
	for (int i = 0; i < used_count; i++) {
		pr_crit("[mpool]   [%d] %s\r\n", i,
			owners[i] ? owners[i] : "unknown");
	}
}
