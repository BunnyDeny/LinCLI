#include "cli_io.h"
#include <stdarg.h>
#include <stdio.h>

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
	return "[test]";
}

char buffer[128];
char tmp[256];

int cli_printk(const char *fmt, ...)
{
	int status;
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	char pre[2] = { buffer[0], '\0' };
	const char *_pre = prefiex_gen(pre);
	snprintf(tmp, sizeof(tmp), "%s %s", _pre, buffer);

	if (len > 0) {
		status = cli_out_push((_u8 *)tmp, len);
		if (status < 0) {
			return status;
		}
	} else {
		return len;
	}
	return 0;
}
