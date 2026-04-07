#include "stateM.h"
#include "cli_io.h"
#include "init_d.h"

extern struct tState _scheduler_start;
extern struct tState _scheduler_end;

struct tStateEngine scheduler_eng;

void first_init_d(void *private)
{
	pr_info("%s", (char *)private);
}
_EXPORT_INIT_SYMBOL(init_d1, "This is the first my_init_d", first_init_d);
_EXPORT_INIT_SYMBOL(init_d2, "This is the first my_init_d", first_init_d);
_EXPORT_INIT_SYMBOL(init_d3, "This is the first my_init_d", first_init_d);

void start_entry(void *private)
{
	pr_info("[scheduler]初始化用户的一些应用\n");

	struct init_d *p_init_d;
	pr_debug("_init_d_start: %p\n", _init_d_start);
	pr_debug("_init_d_end: %p\n", _init_d_end);
	for ((p_init_d) = (_init_d_start); (p_init_d) < (_init_d_end);
	     (p_init_d)++) {
		pr_info("%p\n", p_init_d);
		if (p_init_d && p_init_d->_init_entry) {
			p_init_d->_init_entry(p_init_d->_private);
		}
	}
}

int start_task(void *private)
{
	int status = state_switch(&scheduler_eng, "scheduler_idle");
	if (status) {
		pr_crit("[scheduler]切换空闲任务异常\n");
		return status;
	}
	return 0;
}

void start_exit(void *private)
{
	pr_notice("[scheduler]调度器启动程序执行完毕，将进入空闲状态\n");
}

_EXPORT_STATE_SYMBOL(scheduler_start, start_entry, start_task, start_exit,
		     ".scheduler");

void scheduler_idle_entry(void *private)
{
	pr_info("[scheduler]进入空闲状态 scheduler_idle_entry\n");
	pr_info("[scheduler]空闲状态的行为是回显用户的按键输入\n");
}

int scheduler_idle_task(void *private)
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

_EXPORT_STATE_SYMBOL(scheduler_idle, scheduler_idle_entry, scheduler_idle_task,
		     NULL, ".scheduler");

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
	pr_notice("[scheduler]调度器初始化成功\n");
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
	if (cli_out_sync()) {
		return -2;
	}
	return 0;
}