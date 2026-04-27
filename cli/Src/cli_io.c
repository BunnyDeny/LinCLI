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
 *  loglevel 命令：控制日志过滤级别
 * ============================================================ */

struct loglevel_args {
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

static int loglevel_handler(void *_args)
{
	struct loglevel_args *args = _args;
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

CLI_COMMAND(loglevel, "loglevel", "Set log level filter", loglevel_handler,
	    (struct loglevel_args *)0,
	    OPTION(0, "emerg", BOOL, "Set log level to EMERG (0)",
		   struct loglevel_args, emerg, 0, NULL, NULL, false),
	    OPTION(0, "alert", BOOL, "Set log level to ALERT (1)",
		   struct loglevel_args, alert, 0, NULL, NULL, false),
	    OPTION(0, "crit", BOOL, "Set log level to CRIT (2)",
		   struct loglevel_args, crit, 0, NULL, NULL, false),
	    OPTION(0, "err", BOOL, "Set log level to ERR (3)",
		   struct loglevel_args, err, 0, NULL, NULL, false),
	    OPTION(0, "warning", BOOL, "Set log level to WARNING (4)",
		   struct loglevel_args, warning, 0, NULL, NULL, false),
	    OPTION(0, "notice", BOOL, "Set log level to NOTICE (5)",
		   struct loglevel_args, notice, 0, NULL, NULL, false),
	    OPTION(0, "info", BOOL, "Set log level to INFO (6)",
		   struct loglevel_args, info, 0, NULL, NULL, false),
	    OPTION(0, "debug", BOOL, "Set log level to DEBUG (7)",
		   struct loglevel_args, debug, 0, NULL, NULL, false),
	    END_OPTIONS);
