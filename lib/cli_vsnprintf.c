/*
 * LinCLI - Lightweight vsnprintf for embedded targets (32/64-bit compatible).
 * Supports: %% %d %i %u %x %X %s %c %p %ld %lu %lx %zu %zx
 */

#include "cli_vsnprintf.h"
#include <stdint.h>
#include <stddef.h>

static int _utoa64(uint64_t val, char *buf, int base, int upper)
{
	char tmp[24];
	int i = 0, j;
	if (val == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return 1;
	}
	while (val) {
		int digit = val % base;
		tmp[i++] = (digit < 10) ? ('0' + digit)
					: (upper ? 'A' : 'a') + digit - 10;
		val /= base;
	}
	for (j = 0; j < i; j++)
		buf[j] = tmp[i - 1 - j];
	buf[i] = '\0';
	return i;
}

static int _itoa64(int64_t val, char *buf, int base, int upper)
{
	if (val < 0) {
		buf[0] = '-';
		return 1 + _utoa64((uint64_t)(-val), buf + 1, base, upper);
	}
	return _utoa64((uint64_t)val, buf, base, upper);
}

static int _out_str(char *buf, int buf_size, int *len, const char *s,
		    int width, int left)
{
	int sl = 0;
	while (s[sl])
		sl++;
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
	return sl;
}

static void _out_num(char *buf, int buf_size, int *len, const char *num,
		     int width, int left, char pad)
{
	int nl = 0;
	while (num[nl])
		nl++;
	int pad_count = width - nl;
	if (!left && pad_count > 0) {
		while (pad_count-- > 0 && *len < buf_size - 1)
			buf[(*len)++] = pad;
	}
	while (*num && *len < buf_size - 1)
		buf[(*len)++] = *num++;
	if (left && pad_count > 0) {
		while (pad_count-- > 0 && *len < buf_size - 1)
			buf[(*len)++] = ' ';
	}
}

int cli_vsnprintf(char *buf, int buf_size, const char *fmt, va_list args)
{
	int len = 0;
	char num_buf[32];

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
		char pad = ' ';
		char len_mod = 0;

		while (*fmt == '-' || *fmt == '0') {
			if (*fmt == '-')
				left = 1;
			if (*fmt == '0')
				pad = '0';
			fmt++;
		}
		while (*fmt >= '0' && *fmt <= '9') {
			width = width * 10 + (*fmt - '0');
			fmt++;
		}
		if (*fmt == 'l' || *fmt == 'z') {
			len_mod = *fmt++;
			if (len_mod == 'l' && *fmt == 'l') {
				len_mod = 'L';
				fmt++;
			}
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
			_out_str(buf, buf_size, &len, s ? s : "(null)", width,
				 left);
		} else if (type == 'd' || type == 'i') {
			int64_t val;
			if (len_mod == 'l')
				val = va_arg(args, long);
			else if (len_mod == 'L')
				val = va_arg(args, long long);
			else
				val = va_arg(args, int);
			_itoa64(val, num_buf, 10, 0);
			_out_num(buf, buf_size, &len, num_buf, width, left, pad);
		} else if (type == 'u') {
			uint64_t val;
			if (len_mod == 'l')
				val = va_arg(args, unsigned long);
			else if (len_mod == 'L')
				val = va_arg(args, unsigned long long);
			else
				val = va_arg(args, unsigned int);
			_utoa64(val, num_buf, 10, 0);
			_out_num(buf, buf_size, &len, num_buf, width, left, pad);
		} else if (type == 'x' || type == 'X') {
			uint64_t val;
			if (len_mod == 'l')
				val = va_arg(args, unsigned long);
			else if (len_mod == 'L')
				val = va_arg(args, unsigned long long);
			else if (len_mod == 'z')
				val = va_arg(args, size_t);
			else
				val = va_arg(args, unsigned int);
			_utoa64(val, num_buf, 16, type == 'X');
			_out_num(buf, buf_size, &len, num_buf, width, left, pad);
		} else if (type == 'p') {
			uintptr_t val = (uintptr_t)va_arg(args, void *);
			_utoa64((uint64_t)val, num_buf, 16, 0);
			if (len < buf_size - 1)
				buf[len++] = '0';
			if (len < buf_size - 1)
				buf[len++] = 'x';
			for (int i = 0; num_buf[i] && len < buf_size - 1; i++)
				buf[len++] = num_buf[i];
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
