#include "stateM.h"
#include "cli_io.h"
#include "init_d.h"
#include <string.h>
#include "cli_cmd_line.h"
#include "cmd_dispose.h"

struct tStateEngine scheduler_eng;
extern struct tState _scheduler_start;
extern struct tState _scheduler_end;

void start_entry(void *private)
{
	pr_info("执行_EXPORT_INIT_SYMBOL导出的初始化程序\n");
	CALL_INIT_D;
}

int start_task(void *private)
{
	int status = state_switch(&scheduler_eng, "scheduler_get_char");
	if (status < 0) {
		pr_crit("[scheduler]切换空闲任务异常\n");
		return status;
	}
	return 0;
}

_EXPORT_STATE_SYMBOL(scheduler_start, start_entry, start_task, NULL,
		     ".scheduler");

void scheduler_get_char_entry(void *private)
{
	int status;
	status = state_switch(&cmd_line_mec, "cmd_line_start");
	if (status < 0) {
		pr_emerg("scheduler_get_char_entry切换cmd_line_start异常\n");
		return;
	}
	reset_cli_in_push_lock();
}
int scheduler_get_char_task(void *private)
{
	int status, size;
	size = cli_get_in_size();
	if (size) {
		char ch;
		status = cli_in_pop((_u8 *)&ch, 1);
		if (status < 0) {
			return status;
		}
		status = cli_cmd_line_task(ch);
		if (status < 0) {
			return status;
		} else if (status == cmd_line_enter_press) {
			status = state_switch(&scheduler_eng,
					      "schedule_dispose");
			if (status < 0) {
				return status;
			}
			return 0;
		}
	}
	return 0;
}
void scheduler_get_char_exit(void *arg)
{
	set_cli_in_push_lock();
}
_EXPORT_STATE_SYMBOL(scheduler_get_char, scheduler_get_char_entry,
		     scheduler_get_char_task, scheduler_get_char_exit,
		     ".scheduler");

void scheduler_dispose_entry(void *arg)
{
	engine_init(&dispose_mec, "dispose_start", &_dispose_start,
		    &_dispose_end);
}
int scheduler_dispose_task(void *arg)
{
	int status;
	status = dispose_task(origin_cmd.buf);
	if (status < 0) {
		return status;
	} else if (status == dispose_exit) {
		status = state_switch(&scheduler_eng, "scheduler_get_char");
		if (status < 0) {
			return status;
		}
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(schedule_dispose, scheduler_dispose_entry,
		     scheduler_dispose_task, NULL, ".scheduler");

int scheduler_init(void)
{
	int status;
	cli_io_init();
	status = engine_init(&scheduler_eng, "scheduler_start",
			     &_scheduler_start, &_scheduler_end);
	if (status < 0) {
		pr_emerg("调度器初始化异常，请检查调度器状态机\n");
		return status;
	}
	pr_info("[scheduler]调度器初始化成功\n");
	return 0;
}

/* Test function */
int scheduler_task(void)
{
	int status;
	status = stateEngineRun(&scheduler_eng, NULL);
	if (status < 0) {
		return status;
	}
	if (cli_out_sync()) {
		return -2;
	}
	return 0;
}