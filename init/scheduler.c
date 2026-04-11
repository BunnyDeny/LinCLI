#include "stateM.h"
#include "cli_io.h"
#include "init_d.h"
#include <string.h>
#include "cli_cmd_line.h"

extern struct tState _scheduler_start;
extern struct tState _scheduler_end;

struct tStateEngine scheduler_eng;

void start_entry(void *private)
{
	pr_info("执行_EXPORT_INIT_SYMBOL导出的初始化程序\n");
	CALL_INIT_D;
}

int start_task(void *private)
{
	int status = state_switch(&scheduler_eng, "scheduler_state");
	if (status < 0) {
		pr_crit("[scheduler]切换空闲任务异常\n");
		return status;
	}
	return 0;
}

_EXPORT_STATE_SYMBOL(scheduler_start, start_entry, start_task, NULL,
		     ".scheduler");

void scheduler_entry(void *private)
{
	reset_cli_in_push_lock();
	pr_info("cli_in_push接口已解锁, 开始接受用户按键输入\n");
}

int _scheduler_task(void *private)
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
		}
	}
	return 0;
}

_EXPORT_STATE_SYMBOL(scheduler_state, scheduler_entry, _scheduler_task, NULL,
		     ".scheduler");

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