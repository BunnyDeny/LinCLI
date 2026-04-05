#include "cli_io.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

struct cli_io _cli_io = {
	.in_push_ref = 0,
	.in_pop_ref = 0,
	.out_push_ref = 0,
	.out_pop_ref = 0,
};

void cli_io_init(void)
{
	vectorInit(&_cli_io.in, (_u8 *)_cli_io.in_buf, CLI_IO_SIZE);
	vectorInit(&_cli_io.out, (_u8 *)_cli_io.out_buf, CLI_IO_SIZE);
	_cli_io.in_push_ref = 1;
	_cli_io.in_pop_ref = 1;
	_cli_io.out_push_ref = 1;
	_cli_io.out_pop_ref = 1;
}

//移植的时候实现
void cli_out_sync(void)
{
	while (cli_get_out_size() > 0) {
		char ch;
		int status = cli_out_pop((_u8 *)&ch, 1);
		if (status == 0) {
			write(STDOUT_FILENO, &ch, 1);
		} else {
			printf("cli_out_pop err %d\n", status);
		}
	}
}

__attribute__((weak)) const char *pre_EMERG_gen(void)
{
	return COLOR_RED "[EMERG]";
}

__attribute__((weak)) const char *pre_ALERT_gen(void)
{
	return COLOR_MAGENTA "[ALERT]";
}

__attribute__((weak)) const char *pre_CRIT_gen(void)
{
	return COLOR_BOLD COLOR_RED "[CRIT]";
}

__attribute__((weak)) const char *pre_ERR_gen(void)
{
	return COLOR_RED "[ERR]";
}

__attribute__((weak)) const char *pre_WARNING_gen(void)
{
	return COLOR_YELLOW "[WARNING]";
}

__attribute__((weak)) const char *pre_NOTICE_gen(void)
{
	return COLOR_BLUE "[NOTICE]";
}

__attribute__((weak)) const char *pre_INFO_gen(void)
{
	return COLOR_GREEN "[INFO]";
}

__attribute__((weak)) const char *pre_DEBUG_gen(void)
{
	return COLOR_CYAN "[DEBUG]";
}

__attribute__((weak)) const char *pre_DEFAULT_gen(void)
{
	return "";
}

__attribute__((weak)) const char *pre_CONT_gen(void)
{
	return COLOR_CYAN "[CONT]";
}

static const char *prefiex_gen(const char *level)
{
	char lv = level[0];
	const char *prefiex;
	switch (lv) {
	case '0':
		prefiex = pre_EMERG_gen();
		break;
	case '1':
		prefiex = pre_ALERT_gen();
		break;
	case '2':
		prefiex = pre_CRIT_gen();
		break;
	case '3':
		prefiex = pre_ERR_gen();
		break;
	case '4':
		prefiex = pre_WARNING_gen();
		break;
	case '5':
		prefiex = pre_NOTICE_gen();
		break;
	case '6':
		prefiex = pre_INFO_gen();
		break;
	case '7':
		prefiex = pre_DEBUG_gen();
		break;
	case 'c':
		prefiex = pre_CONT_gen();
		break;
	default:
		prefiex = pre_DEFAULT_gen();
		break;
	}
	return prefiex;
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
	const char *_pre = prefiex_gen(pre);
	if (is_kern_level(buffer[0])) {
		memmove(buffer, buffer + 1, CLI_PRINTK_BUF_SIZE - 1);
	}
	int pre_len = strlen(_pre);
	if (len > 0 && pre_len >= 0) {
		memmove(buffer + pre_len, buffer, len + 1);
		memcpy(buffer, _pre, pre_len);
		strcat(buffer, COLOR_NONE);
		status = cli_out_push((_u8 *)buffer,
				      pre_len + len + strlen(COLOR_NONE));
		if (status)
			return status;
		cli_out_sync();
	}
	return len;
}
