#include "stateM.h"
#include "cli_io.h"

extern struct tState _scheduler_start;
extern struct tState _scheduler_end;

struct tStateEngine scheduler_eng;

/* State functions implementation */
void cli_idle_entry(void *private)
{
	cli_io_init();
	cli_printk("[scheduler] 进入空闲状态 cli_idle_entry\n");
}

int cli_idle_task(void *private)
{
	int status, size;
	size = cli_get_in_size();
	if (size) {
		char ch;
		status = cli_in_pop((_u8 *)&ch, 1);
		if (status) {
			return status;
		}
		status = cli_out_push((_u8 *)&ch, 1);
		if (status) {
			return status;
		}
	}
	return 0;
}

_EXPORT_STATE_SYMBOL(cli_idle, cli_idle_entry, cli_idle_task, NULL, ".state");

int scheduler_init(void)
{
	int status;
	status = engine_init(&scheduler_eng, "cli_idle", &_scheduler_start,
			     &_scheduler_end);
	if (status < 0) {
		return status;
	}
	return 0;
}

/* Test function */
int scheduler_task(void)
{
	int status;
	status = stateEngineRun(&scheduler_eng, NULL);
	if (status < 0) {
		return status;
	} else if (status == 1) {
		return status;
	}
	return 0;
}