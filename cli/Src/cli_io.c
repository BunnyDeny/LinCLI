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

char buffer[128];
int _cli_io_printf(const char *fmt, ...)
{
	int status;
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	if (len > 0) {
		status = cli_out_push((_u8 *)buffer, len);
		if (status < 0) {
			return status;
		}
	} else {
		return len;
	}
	return 0;
}
