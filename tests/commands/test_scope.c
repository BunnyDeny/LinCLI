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
#include <math.h>

struct scope_args {
	char *vars;
	int period_ms;
	int duration_s;
};

static int scope_tick = 0;

/* Simple simulated motor data using integer math (scaled x1000) */
static void scope_generate_data(int *timestamp,
				int *theta,
				int *speed,
				int *iq)
{
	/* theta: 0 ~ 6283 (0 ~ 2*pi, scaled x1000) */
	*timestamp = scope_tick * 10; /* ms */
	*theta = (scope_tick * 628) % 6283;
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
