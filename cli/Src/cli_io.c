#include "cli_io.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

__attribute__((weak)) const char *pre_EMERG_gen(void)
{
	return "[EMERG]";
}

__attribute__((weak)) const char *pre_ALERT_gen(void)
{
	return "[ALERT]";
}

__attribute__((weak)) const char *pre_CRIT_gen(void)
{
	return "[CRIT]";
}

__attribute__((weak)) const char *pre_ERR_gen(void)
{
	return "[ERR]";
}

__attribute__((weak)) const char *pre_WARNING_gen(void)
{
	return "[WARNING]";
}

__attribute__((weak)) const char *pre_NOTICE_gen(void)
{
	return "[NOTICE]";
}

__attribute__((weak)) const char *pre_INFO_gen(void)
{
	return "[INFO]";
}

__attribute__((weak)) const char *pre_DEBUG_gen(void)
{
	return "[DEBUG]";
}

__attribute__((weak)) const char *pre_DEFAULT_gen(void)
{
	return "";
}

__attribute__((weak)) const char *pre_CONT_gen(void)
{
	return "[CONT]";
}

static const char *prefiex_gen(const char *level)
{
	char lv = level[0];
	const char *prefiex;
	switch (lv) {
	case KERN_EMERG:
		prefiex = pre_EMERG_gen();
		break;
	case KERN_ALERT:
		prefiex = pre_ALERT_gen();
		break;
	case KERN_CRIT:
		prefiex = pre_CRIT_gen();
		break;
	case KERN_ERR:
		prefiex = pre_ERR_gen();
		break;
	case KERN_WARNING:
		prefiex = pre_WARNING_gen();
		break;
	case KERN_NOTICE:
		prefiex = pre_NOTICE_gen();
		break;
	case KERN_INFO:
		prefiex = pre_INFO_gen();
		break;
	case KERN_DEBUG:
		prefiex = pre_DEBUG_gen();
		break;
	case KERN_CONT:
		prefiex = pre_CONT_gen();
		break;
	default:
		prefiex = pre_DEFAULT_gen();
		break;
	}
	return prefiex;
}

static char buffer[256];

int cli_printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	char pre[2] = { buffer[0], '\0' };
	const char *_pre = prefiex_gen(pre);
	int pre_len = strlen(_pre);
	if (len > 0 && pre_len >= 0) {
		memmove(buffer + pre_len, buffer, len + 1);
		memcpy(buffer, _pre, pre_len);
		return cli_out_push((_u8 *)buffer, pre_len + len);
	}
	return len;
}
