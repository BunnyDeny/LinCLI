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

static char buffer[CLI_PRINTK_BUF_SIZE];

int cli_printk(const char *fmt, ...)
{
	int status;
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	char pre[2] = { buffer[0], '\0' };
	if (pre[0] != '8' && pre[0] >= '0' && pre[0] <= '7') {
		if (pre[0] > log_level[0]) {
			return 0;
		}
	}
	if ((!is_kern_level(pre[0]) || !strcmp(pre, KERN_CONT)) &&
	    strcmp("8", log_level)) {
		return 0;
	}
	const char *_pre = prefix_gen(pre);
	if (is_kern_level(buffer[0])) {
		memmove(buffer, buffer + 1, CLI_PRINTK_BUF_SIZE - 1);
	}
	int pre_len = strlen(_pre);
	if (len > 0 && pre_len >= 0) {
		memmove(buffer + pre_len, buffer, len + 1);
		memcpy(buffer, _pre, pre_len);
		strcat(buffer, COLOR_NONE);
		if (cli_out_sync())
			return CLI_ERR_IO_SYNC;
		status = cli_out_push((_u8 *)buffer,
				      pre_len + len + strlen(COLOR_NONE));
		if (status < 0)
			return status;
		if (cli_out_sync())
			return CLI_ERR_IO_SYNC;
	}
	return len;
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
