/*
 * LinCLI - Size measurement stub.
 * Minimal entry point to keep LinCLI core functions from being GC'd.
 */

#include "cli_io.h"

extern int scheduler_init(void);
extern int scheduler_task(void);

/* Weak symbols referenced by scheduler.c — provide empty defaults */
const char *const cli_auto_cmds[] = { NULL };
const int cli_auto_cmds_count = 0;

void _exit(int status)
{
	(void)status;
	while (1) {}
}

int main(void)
{
	scheduler_init();
	scheduler_task();
	return 0;
}
