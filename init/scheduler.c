/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "stateM.h"
#include "cli_io.h"
#include "init_d.h"
#include <string.h>
#include "cli_cmd_line.h"
#include "cmd_dispose.h"
#include "cli_auto_cmd.h"

struct tStateEngine scheduler_eng;
extern struct tState *const _scheduler_start[];
extern struct tState *const _scheduler_end[];

__attribute__((weak)) void cli_prompt_print(void)
{
	cli_printk(COLOR_BOLD COLOR_GREEN
		   "lin" COLOR_NONE COLOR_BOLD
		   "@" COLOR_NONE COLOR_BOLD COLOR_CYAN
		   "linCli" COLOR_NONE COLOR_BOLD COLOR_YELLOW "> " COLOR_NONE);
}

void start_entry(void *private)
{
	pr_info("执行_EXPORT_INIT_SYMBOL导出的初始化程序\r\n");
	CALL_INIT_D;
}
int start_task(void *private)
{
	int status = state_switch(&scheduler_eng, "scheduler_auto_run");
	if (status < 0) {
		pr_crit("[scheduler]切换自动运行任务异常\r\n");
		return status;
	}
	return 0;
}
_EXPORT_STATE_SYMBOL(scheduler_start, start_entry, start_task, NULL,
		     ".scheduler");

void scheduler_get_char_entry(void *private)
{
	int status = cli_cmd_line_init();
	if (status < 0) {
		pr_emerg("cli_cmd_line_init异常\r\n");
	}
	cli_printk("\r\n");
	cli_prompt_print();
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

static int auto_run_idx = 0;

void scheduler_auto_run_entry(void *private)
{
	auto_run_idx = 0;
}

int scheduler_auto_run_task(void *private)
{
	extern int g_last_cmd_ret;

	if (!cli_auto_cmds || cli_auto_cmds_count <= 0) {
		int status = state_switch(&scheduler_eng, "scheduler_get_char");
		return status < 0 ? status : 0;
	}

	while (auto_run_idx < cli_auto_cmds_count) {
		const char *cmd = cli_auto_cmds[auto_run_idx];
		if (!cmd) {
			auto_run_idx++;
			continue;
		}

		int len = strlen(cmd);
		if (len >= CMD_LINE_BUF_SIZE)
			len = CMD_LINE_BUF_SIZE - 1;
		memset(origin_cmd.buf, 0, CMD_LINE_BUF_SIZE);
		memcpy(origin_cmd.buf, cmd, len);
		origin_cmd.size = len;

		int status = dispose_task(origin_cmd.buf);
		if (status < 0) {
			return status;
		}

		if (g_last_cmd_ret < 0) {
			pr_err("自动命令执行失败，停止后续执行\r\n");
			int s = state_switch(&scheduler_eng,
					     "scheduler_get_char");
			return s < 0 ? s : 0;
		}

		auto_run_idx++;
	}

	int status = state_switch(&scheduler_eng, "scheduler_get_char");
	return status < 0 ? status : 0;
}
_EXPORT_STATE_SYMBOL(scheduler_auto_run, scheduler_auto_run_entry,
		     scheduler_auto_run_task, NULL, ".scheduler");

int scheduler_dispose_task(void *arg)
{
	int status;
	(void)arg;

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
_EXPORT_STATE_SYMBOL(schedule_dispose, NULL, scheduler_dispose_task, NULL,
		     ".scheduler");

int scheduler_init(void)
{
	int status;
	cli_io_init();
	status = engine_init(&scheduler_eng, "scheduler_start",
			     _scheduler_start, _scheduler_end);
	if (status < 0) {
		pr_emerg("调度器初始化异常，请检查调度器状态机\r\n");
		return status;
	}
	pr_info("[scheduler]调度器初始化成功\r\n");
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
