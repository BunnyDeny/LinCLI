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

#if CLI_ENABLE_TESTS
#include "cmd_dispose.h"
#include "cli_io.h"

struct scope_args {
	char *vars;
	int period_ms;
	int duration_s;
};

static int scope_tick = 0;

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

static void scope_generate_data(int *timestamp,
					int *theta,
					int *speed,
					int *iq)
{
	*timestamp = scope_tick * 10; /* ms */
	/* theta: sine wave via 64-point lookup table */
	*theta = sin_table[scope_tick % 64];
	/* speed: 1000 ~ 2000 (三角波) */
	int phase = scope_tick % 200;
	if (phase < 100)
		*speed = 1000 + phase * 10;
	else
		*speed = 2000 - (phase - 100) * 10;
	/* iq: 500 ~ 3500 (另一个三角波) */
	int phase2 = scope_tick % 134;
	if (phase2 < 67)
		*iq = 500 + phase2 * 45;
	else
		*iq = 3500 - (phase2 - 67) * 45;
}

static void scope_entry(void *_args)
{
	struct scope_args *args = _args;
	(void)args;
	scope_tick = 0;
	/* Unlock input buffer so we can catch Ctrl+D during scope run */
	reset_cli_in_push_lock();
	/* Notify host to reset CSV and open a new plot window */
	cli_printk("\r$SCOPE_START\r\n");
	/* Print CSV header (normal line, not \r prefixed) */
	cli_printk("timestamp,theta,speed,iq\r\n");
}

static int scope_task(void *_args)
{
	struct scope_args *args = _args;
	int timestamp, theta, speed, iq;

	/* Check for Ctrl+D (ASCII 4) to exit */
	if (cli_get_in_size() > 0) {
		char ch;
		int status = cli_in_pop((_u8 *)&ch, 1);
		if (status > 0 && ch == (char)4) {
			cli_printk("\r\n");
			return 0;
		}
	}

	scope_generate_data(&timestamp, &theta, &speed, &iq);

	/* \r brings cursor to line start, single-line refresh.
	 * Values are scaled x1000; Python side divides by 1000.
	 */
	cli_printk("\r %d,%d,%d,%d", timestamp, theta, speed, iq);

	scope_tick++;

	if (args->duration_s > 0 &&
	    scope_tick * args->period_ms >= args->duration_s * 1000)
		return 0;

	return CLI_CONTINUE;
}

CLI_COMMAND_ASYNC(scope, "scope", "Real-time data scope for CSV bridge",
		  USAGE("scope [-p <ms>] [-d <s>]"),
		  scope_entry, scope_task, NULL,
		  (struct scope_args *)0,
		  OPTION('p', "period", INT, "Sample period in ms (default 100)",
			 struct scope_args, period_ms, 0, NULL, NULL, false),
		  OPTION('d', "duration", INT, "Duration in seconds, 0=forever",
			 struct scope_args, duration_s, 0, NULL, NULL, false),
		  END_OPTIONS);
#endif
