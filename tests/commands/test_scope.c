/*
 * LinCLI - Real-time scope demo for CSV data bridge.
 *
 * Outputs \r-prefixed data lines that can be captured by
 * tools/lincli_csv_bridge.py for live plotting.
 *
 * Note: cli_vsnprintf only supports %% %d %u %s %c,
 * so floating-point values are scaled to integers (x1000).
 */

#include "cli_config.h"

#if CLI_ENABLE_DEMO_COMMANDS
#include "cmd_dispose.h"
#include "cli_io.h"


/* 64-point sine lookup table: 0 ~ 6283 (scaled x1000) */
static const int sin_table[64] = {
	3141, 3449, 3754, 4053, 4343, 4622, 4886, 5134,
	5362, 5569, 5753, 5912, 6043, 6147, 6222, 6267,
	6283, 6267, 6222, 6147, 6043, 5912, 5753, 5569,
	5362, 5134, 4886, 4622, 4343, 4053, 3754, 3449,
	3141, 2833, 2528, 2229, 1939, 1660, 1396, 1148,
	 920,  713,  529,  370,  239,  135,   60,   15,
	   0,   15,   60,  135,  239,  370,  529,  713,
	 920, 1148, 1396, 1660, 1939, 2229, 2528, 2833,
};

static int scope_check_exit(void)
{
	if (cli_get_in_size() > 0) {
		char ch;
		int status = cli_in_pop((_u8 *)&ch, 1);
		if (status > 0 && ch == (char)4) {
			cli_printk("\r\n");
			return 1;
		}
	}
	return 0;
}

static void scope_compute(int tick, int *theta, int *speed, int *iq)
{
	int phase, phase2;

	/* theta: sine wave via 64-point lookup table */
	*theta = sin_table[tick % 64];
	/* speed: 1000 ~ 2000 (triangular wave) */
	phase = tick % 200;
	if (phase < 100)
		*speed = 1000 + phase * 10;
	else
		*speed = 2000 - (phase - 100) * 10;
	/* iq: 500 ~ 3500 (another triangular wave) */
	phase2 = tick % 134;
	if (phase2 < 67)
		*iq = 500 + phase2 * 45;
	else
		*iq = 3500 - (phase2 - 67) * 45;
}

static void scope_entry(void *_args)
{
	(void)_args;
	/* Unlock input buffer so we can catch Ctrl+D during scope run */
	reset_cli_in_push_lock();
	/* Notify host to reset CSV and open a new plot window */
	cli_printk("\r$SCOPE_START\r\n");
	/* Print CSV header (normal line, not \r prefixed) */
	cli_printk("ch1,ch2,ch3\r\n");
}

static int scope_task(void *_args)
{
	static int tick = 0;
	int theta, speed, iq;

	(void)_args;

	if (scope_check_exit())
		return 0;

	scope_compute(tick, &theta, &speed, &iq);

	/* \r brings cursor to line start, single-line refresh.
	 * Values are scaled x1000; Python side divides by 1000.
	 */
	cli_printk("\r%d,%d,%d", theta, speed, iq);

	tick++;
	return CLI_CONTINUE;
}

CLI_COMMAND_ASYNC(scope, "scope", "Real-time data scope for CSV bridge",
		  USAGE("scope"),
		  scope_entry, scope_task, NULL,
		  NULL,
		  END_OPTIONS);
#endif
