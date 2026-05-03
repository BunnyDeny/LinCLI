/*
 * LinCLI - Minimal vsnprintf for embedded targets.
 * Supports: %% %d %u %s %c (with width/left-align)
 */

#include "cli_vsnprintf.h"
#include <stdint.h>
#include <stddef.h>

static int _utoa(unsigned int val, char *buf)
{
	char tmp[12];
	int i = 0, j;
	if (val == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return 1;
	}
	while (val) {
		tmp[i++] = '0' + (val % 10);
		val /= 10;
	}
	for (j = 0; j < i; j++)
		buf[j] = tmp[i - 1 - j];
	buf[i] = '\0';
	return i;
}

static int _itoa(int val, char *buf)
{
	if (val < 0) {
		buf[0] = '-';
		return 1 + _utoa((unsigned int)(-val), buf + 1);
	}
	return _utoa((unsigned int)val, buf);
}

static void _out_str(char *buf, int buf_size, int *len, const char *s, int width, int left)
{
	int sl = 0;
	while (s[sl]) sl++;
	int pad = width - sl;
	if (!left) {
		while (pad-- > 0 && *len < buf_size - 1)
			buf[(*len)++] = ' ';
	}
	while (*s && *len < buf_size - 1)
		buf[(*len)++] = *s++;
	if (left) {
		while (pad-- > 0 && *len < buf_size - 1)
			buf[(*len)++] = ' ';
	}
}

static void _out_num(char *buf, int buf_size, int *len, const char *num, int width, int left)
{
	int nl = 0;
	while (num[nl]) nl++;
	int pad = width - nl;
	if (!left) {
		while (pad-- > 0 && *len < buf_size - 1)
			buf[(*len)++] = ' ';
	}
	while (*num && *len < buf_size - 1)
		buf[(*len)++] = *num++;
	if (left) {
		while (pad-- > 0 && *len < buf_size - 1)
			buf[(*len)++] = ' ';
	}
}

int cli_vsnprintf(char *buf, int buf_size, const char *fmt, va_list args)
{
	int len = 0;
	char num_buf[12];

	if (buf_size <= 0)
		return 0;

	while (*fmt && len < buf_size - 1) {
		if (*fmt != '%') {
			buf[len++] = *fmt++;
			continue;
		}
		fmt++;
		int left = 0;
		int width = 0;

		if (*fmt == '-') {
			left = 1;
			fmt++;
		}
		while (*fmt >= '0' && *fmt <= '9') {
			width = width * 10 + (*fmt - '0');
			fmt++;
		}

		char type = *fmt++;
		if (type == '%') {
			buf[len++] = '%';
		} else if (type == 'c') {
			char ch = (char)va_arg(args, int);
			if (!left && width > 1) {
				int pc = width - 1;
				while (pc-- > 0 && len < buf_size - 1)
					buf[len++] = ' ';
			}
			buf[len++] = ch;
			if (left && width > 1) {
				int pc = width - 1;
				while (pc-- > 0 && len < buf_size - 1)
					buf[len++] = ' ';
			}
		} else if (type == 's') {
			const char *s = va_arg(args, const char *);
			_out_str(buf, buf_size, &len, s ? s : "(null)", width, left);
		} else if (type == 'd' || type == 'i') {
			int val = va_arg(args, int);
			_itoa(val, num_buf);
			_out_num(buf, buf_size, &len, num_buf, width, left);
		} else if (type == 'u') {
			unsigned int val = va_arg(args, unsigned int);
			_utoa(val, num_buf);
			_out_num(buf, buf_size, &len, num_buf, width, left);
		} else {
			buf[len++] = '%';
			if (len < buf_size - 1)
				buf[len++] = type;
		}
	}

	buf[len] = '\0';
	return len;
}

int cli_snprintf(char *buf, int buf_size, const char *fmt, ...)
{
	va_list args;
	int len;
	va_start(args, fmt);
	len = cli_vsnprintf(buf, buf_size, fmt, args);
	va_end(args);
	return len;
}
