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
#include "cli_errno.h"
#include "cli_io.h"
#include "init_d.h"
#include <string.h>
#include "cli_cmd_line.h"
#include "cmd_dispose.h"
#include "cli_auto_cmd.h"
#include "cli_mpool.h"

struct tStateEngine scheduler_eng;
extern struct tState *const _scheduler_start[];
extern struct tState *const _scheduler_end[];

__attribute__((weak)) void cli_prompt_print(void)
{
	all_printk(COLOR_BOLD COLOR_GREEN
		   "lin" COLOR_NONE COLOR_BOLD
		   "@" COLOR_NONE COLOR_BOLD COLOR_CYAN
		   "linCli" COLOR_NONE COLOR_BOLD COLOR_YELLOW "> " COLOR_NONE);
}

void start_entry(void *private)
{
	pr_info("execute initialization routines"
		" exported by _EXPORT_INIT_SYMBOL\r\n");
	CALL_INIT_D;
}
int start_task(void *private)
{
	int status = state_switch(&scheduler_eng, "scheduler_auto_run");
	if (status < 0) {
		pr_crit("[scheduler] failed to switch \
			auto-run task, error code: %d\r\n",
			status);
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(scheduler_start, start_entry, start_task, NULL,
		     ".scheduler");

void scheduler_get_char_entry(void *private)
{
	int status = cli_cmd_line_init();
	if (status < 0) {
		pr_emerg("cli_cmd_line_init exception\r\n");
	}
	status = cli_in_clear();
	if (status < 0) {
		pr_err("failed to clear input buffer\r\n");
	}
	all_printk("\r\n");
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
					      "scheduler_dispose");
			if (status < 0) {
				return status;
			}
			return CLI_OK;
		}
	}
	return CLI_OK;
}
void scheduler_get_char_exit(void *arg)
{
	set_cli_in_push_lock();
}
_EXPORT_STATE_SYMBOL(scheduler_get_char, scheduler_get_char_entry,
		     scheduler_get_char_task, scheduler_get_char_exit,
		     ".scheduler");

/* ============================================================
 * 命令执行上下文与新增 scheduler_cmd_run 状态
 * ============================================================ */

#define SCHEDULER_CHAIN_MAX 8

struct scheduler_cmd_ctx {
	const cli_command_t *cmd_def;
	int cmd_ret;

	/* 命令链管理 */
	char *chain_buf;
	char *cmds[SCHEDULER_CHAIN_MAX];
	int chain_cnt;
	int chain_idx;

	/* 自启动索引 */
	int auto_run_idx;

	/* 命令执行完后切换到的目标状态名 */
	char next_state[32];

	/* 别名替换缓冲区，在 cmd_run_exit 中释放 */
	char *alias_buf;
};

static struct scheduler_cmd_ctx cmd_ctx;

void scheduler_cmd_run_entry(void *private)
{
	(void)private;
	if (cmd_ctx.cmd_def && cmd_ctx.cmd_def->cmd_entry) {
		cmd_ctx.cmd_def->cmd_entry(cmd_ctx.cmd_def->arg_buf);
	}
}

int scheduler_cmd_run_task(void *private)
{
	(void)private;

	if (!cmd_ctx.cmd_def || !cmd_ctx.cmd_def->cmd_task) {
		return state_switch(&scheduler_eng, cmd_ctx.next_state);
	}

	int ret = cmd_ctx.cmd_def->cmd_task(cmd_ctx.cmd_def->arg_buf);
	cmd_ctx.cmd_ret = ret;

	/* 判断是旧式命令（仅注册了 validator，三阶段为 NULL）还是新式命令 */
	bool is_legacy = (cmd_ctx.cmd_def->cmd_entry == NULL &&
			  cmd_ctx.cmd_def->cmd_exit == NULL);

	if (is_legacy) {
		/* 旧式命令：task 执行一次即结束，返回值已记录 */
		return state_switch(&scheduler_eng, cmd_ctx.next_state);
	}

	if (ret == CLI_CONTINUE) {
		/* 明确请求继续执行，保持当前状态 */
		return CLI_OK;
	}

	/*
	 * ret == 0  : 执行成功，退出命令
	 * ret < 0   : 执行失败，退出命令，错误码已记录在 cmd_ctx.cmd_ret
	 * ret > 1   : 扩展语义，退出命令
	 */
	return state_switch(&scheduler_eng, cmd_ctx.next_state);
}

void scheduler_cmd_run_exit(void *private)
{
	(void)private;
	if (cmd_ctx.cmd_def && cmd_ctx.cmd_def->cmd_exit) {
		cmd_ctx.cmd_def->cmd_exit(cmd_ctx.cmd_def->arg_buf);
	}

	/* 命令执行失败时打印错误码 */
	if (cmd_ctx.cmd_ret < 0 && cmd_ctx.cmd_def) {
		pr_err("command '%s' execution failed, return value: %d\r\n",
		       cmd_ctx.cmd_def->name, cmd_ctx.cmd_ret);
	}

	cmd_parse_cleanup(cmd_ctx.cmd_def);
	cmd_ctx.cmd_def = NULL;

	if (cmd_ctx.alias_buf) {
		cli_mpool_free(cmd_ctx.alias_buf);
		cmd_ctx.alias_buf = NULL;
	}
}

_EXPORT_STATE_SYMBOL(scheduler_cmd_run, scheduler_cmd_run_entry,
		     scheduler_cmd_run_task, scheduler_cmd_run_exit,
		     ".scheduler");

/* ============================================================
 * 自启动命令状态（改造为异步状态驱动）
 * ============================================================ */

int scheduler_auto_run_task(void *private)
{
	(void)private;
	int status;

	/* 上一个自启动命令执行失败，停止后续执行 */
	if (cmd_ctx.auto_run_idx > 0 && cmd_ctx.cmd_ret < 0) {
		cmd_ctx.auto_run_idx = 0;
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	if (!cli_auto_cmds || cli_auto_cmds_count <= 0) {
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	if (cmd_ctx.auto_run_idx >= cli_auto_cmds_count) {
		/* 自启动全部完成，重置索引并进入交互模式 */
		cmd_ctx.auto_run_idx = 0;
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	const char *cmd = cli_auto_cmds[cmd_ctx.auto_run_idx];
	if (!cmd) {
		cmd_ctx.auto_run_idx++;
		/* 返回 CLI_OK 保持当前状态，下次轮询处理下一个 */
		return CLI_OK;
	}

	int len = strlen(cmd);
	if (len >= CMD_LINE_BUF_SIZE)
		len = CMD_LINE_BUF_SIZE - 1;
	memset(origin_cmd.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(origin_cmd.buf, cmd, len);
	origin_cmd.size = len;

	char *alias_buf = cli_mpool_alloc();
	if (!alias_buf) {
		pr_err("out of memory\r\n");
		cmd_ctx.auto_run_idx++;
		return CLI_OK;
	}

#if ALIAS_EN
	{
		char *replaced = alias_replace(origin_cmd.buf, alias_buf,
					       CLI_MPOOL_SIZE);
		if (replaced != origin_cmd.buf) {
			int repl_len = strlen(replaced);
			if (repl_len < CMD_LINE_BUF_SIZE) {
				memcpy(origin_cmd.buf, replaced, repl_len + 1);
				origin_cmd.size = repl_len;
			}
		}
	}
#endif

	status = cmd_parse_prepare(origin_cmd.buf, &cmd_ctx.cmd_def,
				   &cmd_ctx.cmd_ret);
	if (status < 0) {
		cli_mpool_free(alias_buf);
		/* 解析失败，跳过该命令，下次轮询处理下一个 */
		cmd_ctx.auto_run_idx++;
		return CLI_OK;
	}
	if (status == dispose_exit) {
		cli_mpool_free(alias_buf);
		/* 帮助已打印或无需执行，下次轮询处理下一个 */
		cmd_ctx.auto_run_idx++;
		return CLI_OK;
	}

	/* 准备好后，切换到命令执行状态 */
	cmd_ctx.alias_buf = alias_buf;
	cmd_ctx.auto_run_idx++;
	strncpy(cmd_ctx.next_state, "scheduler_auto_run",
		sizeof(cmd_ctx.next_state));
	return state_switch(&scheduler_eng, "scheduler_cmd_run");
}
_EXPORT_STATE_SYMBOL(scheduler_auto_run, NULL, scheduler_auto_run_task, NULL,
		     ".scheduler");

/* ============================================================
 * 命令分派状态（改造为管理命令链）
 * ============================================================ */

extern int split_cmd_chain(char *buf, char **cmds, int max_cmds);

int scheduler_dispose_task(void *arg)
{
	(void)arg;
	int status;

	/* 首次进入：初始化命令链 */
	if (cmd_ctx.chain_cnt == 0) {
		if (!origin_cmd.buf[0]) {
			return state_switch(&scheduler_eng,
					    "scheduler_get_char");
		}

		cmd_ctx.chain_buf = cli_mpool_alloc();
		if (!cmd_ctx.chain_buf) {
			pr_err("out of memory\r\n");
			return state_switch(&scheduler_eng,
					    "scheduler_get_char");
		}

		int len = origin_cmd.size;
		memcpy(cmd_ctx.chain_buf, origin_cmd.buf, len);
		cmd_ctx.chain_buf[len] = '\0';

		cmd_ctx.chain_cnt = split_cmd_chain(
			cmd_ctx.chain_buf, cmd_ctx.cmds, SCHEDULER_CHAIN_MAX);
		cmd_ctx.chain_idx = 0;
	}

	/* 上一个命令执行失败，中断命令链 */
	if (cmd_ctx.chain_idx > 0 && cmd_ctx.cmd_ret < 0) {
		if (cmd_ctx.chain_buf) {
			cli_mpool_free(cmd_ctx.chain_buf);
			cmd_ctx.chain_buf = NULL;
		}
		cmd_ctx.chain_cnt = 0;
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	/* 命令链已全部执行完毕 */
	if (cmd_ctx.chain_idx >= cmd_ctx.chain_cnt) {
		if (cmd_ctx.chain_buf) {
			cli_mpool_free(cmd_ctx.chain_buf);
			cmd_ctx.chain_buf = NULL;
		}
		cmd_ctx.chain_cnt = 0;
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	/* 准备当前命令 */
	char *current_cmd = cmd_ctx.cmds[cmd_ctx.chain_idx];
	cmd_ctx.chain_idx++;

	char *alias_buf = cli_mpool_alloc();
	if (!alias_buf) {
		pr_err("out of memory\r\n");
		goto chain_failed;
	}

#if ALIAS_EN
	current_cmd = alias_replace(current_cmd, alias_buf, CLI_MPOOL_SIZE);
#endif

	status = cmd_parse_prepare(current_cmd, &cmd_ctx.cmd_def,
				   &cmd_ctx.cmd_ret);
	if (status < 0) {
		cli_mpool_free(alias_buf);
		/* 解析失败，中断命令链 */
		goto chain_failed;
	}
	if (status == dispose_exit) {
		cli_mpool_free(alias_buf);
		/* 无需执行，继续下一个（下次调度直接再来） */
		return CLI_OK;
	}

	cmd_ctx.alias_buf = alias_buf;
	strncpy(cmd_ctx.next_state, "scheduler_dispose",
		sizeof(cmd_ctx.next_state));
	return state_switch(&scheduler_eng, "scheduler_cmd_run");

chain_failed:
	if (cmd_ctx.chain_buf) {
		cli_mpool_free(cmd_ctx.chain_buf);
		cmd_ctx.chain_buf = NULL;
	}
	cmd_ctx.chain_cnt = 0;
	return state_switch(&scheduler_eng, "scheduler_get_char");
}
_EXPORT_STATE_SYMBOL(scheduler_dispose, NULL, scheduler_dispose_task, NULL,
		     ".scheduler");

int scheduler_init(void)
{
	int status;
	cli_io_init();
	status = engine_init(&scheduler_eng, "scheduler_start",
			     _scheduler_start, _scheduler_end);
	if (status < 0) {
		pr_emerg("[scheduler] init failed: %s (%d), \
			check scheduler state machine\r\n",
			 cli_strerror(status), status);
		return status;
	}
	pr_info("[scheduler] initialization successful\r\n");
	return 0;
}

int cnt;
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
	cnt++;
	if ((cnt % 50) == 0)
		pr_info("test\r\n");
	return 0;
}
