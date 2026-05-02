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
#include "cli_var.h"
#include "cli_env.h"

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

struct scheduler_cmd_ctx {
	const cli_command_t *cmd_def;
	int cmd_ret;

	/* 主命令链 */
	char *chain_buf;
	char *chain_p;

	/* 子命令链（环境变量替换产生） */
	char *sub_chain_buf;
	char *sub_chain_p;

	/* 自启动索引 */
	int auto_run_idx;

	/* 命令执行完后切换到的目标状态名 */
	char next_state[32];

	/* 环境变量替换缓冲区，在 cmd_run_exit 中释放 */
	char *env_buf;
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

	bool is_legacy = (cmd_ctx.cmd_def->cmd_entry == NULL &&
			  cmd_ctx.cmd_def->cmd_exit == NULL);

	if (is_legacy) {
		return state_switch(&scheduler_eng, cmd_ctx.next_state);
	}

	if (ret == CLI_CONTINUE) {
		return CLI_OK;
	}

	return state_switch(&scheduler_eng, cmd_ctx.next_state);
}

void scheduler_cmd_run_exit(void *private)
{
	(void)private;
	if (cmd_ctx.cmd_def && cmd_ctx.cmd_def->cmd_exit) {
		cmd_ctx.cmd_def->cmd_exit(cmd_ctx.cmd_def->arg_buf);
	}

	if (cmd_ctx.cmd_ret < 0 && cmd_ctx.cmd_def) {
		pr_err("command '%s' execution failed, return value: %d\r\n",
		       cmd_ctx.cmd_def->name, cmd_ctx.cmd_ret);
	}

	cmd_parse_cleanup(cmd_ctx.cmd_def);
	cmd_ctx.cmd_def = NULL;

	if (cmd_ctx.env_buf) {
		cli_mpool_free(cmd_ctx.env_buf);
		cmd_ctx.env_buf = NULL;
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

	if (cmd_ctx.auto_run_idx > 0 && cmd_ctx.cmd_ret < 0) {
		cmd_ctx.auto_run_idx = 0;
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	if (!cli_auto_cmds || cli_auto_cmds_count <= 0) {
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	if (cmd_ctx.auto_run_idx >= cli_auto_cmds_count) {
		cmd_ctx.auto_run_idx = 0;
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	const char *cmd = cli_auto_cmds[cmd_ctx.auto_run_idx];
	if (!cmd) {
		cmd_ctx.auto_run_idx++;
		return CLI_OK;
	}

	int len = strlen(cmd);
	if (len >= CMD_LINE_BUF_SIZE)
		len = CMD_LINE_BUF_SIZE - 1;
	memset(origin_cmd.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(origin_cmd.buf, cmd, len);
	origin_cmd.size = len;

	char *env_buf = cli_mpool_alloc();
	if (env_buf) {
		int ret = cli_env_replace(origin_cmd.buf, env_buf,
					  CLI_MPOOL_SIZE);
		if (ret == CLI_OK) {
			int env_len = strlen(env_buf);
			if (env_len < CMD_LINE_BUF_SIZE) {
				memcpy(origin_cmd.buf, env_buf, env_len + 1);
				origin_cmd.size = env_len;
			}
		}
		cli_mpool_free(env_buf);
	}

	status = cmd_parse_prepare(origin_cmd.buf, &cmd_ctx.cmd_def,
				   &cmd_ctx.cmd_ret);
	if (status < 0) {
		cmd_ctx.auto_run_idx++;
		return CLI_OK;
	}
	if (status == dispose_exit) {
		cmd_ctx.auto_run_idx++;
		return CLI_OK;
	}

	cmd_ctx.auto_run_idx++;
	strncpy(cmd_ctx.next_state, "scheduler_auto_run",
		sizeof(cmd_ctx.next_state));
	return state_switch(&scheduler_eng, "scheduler_cmd_run");
}
_EXPORT_STATE_SYMBOL(scheduler_auto_run, NULL, scheduler_auto_run_task, NULL,
		     ".scheduler");

/* ============================================================
 * 命令分派状态（支持环境变量展开后的命令链）
 * ============================================================ */

extern int split_cmd_chain(char *buf, char **cmds, int max_cmds);

static char *trim_tail(char *start, char *end)
{
	while (end > start && *end == ' ')
		*end-- = '\0';
	return end;
}

static char *skip_quoted(char *p)
{
	char quote = *p++;

	while (*p && *p != quote)
		p++;
	if (*p == quote)
		p++;
	return p;
}

static char *extract_next_cmd(char **p_out)
{
	char *p = *p_out;

	while (*p == ' ')
		p++;
	if (!*p)
		return NULL;

	char *start = p;
	while (*p) {
		if (*p == '\'' || *p == '"') {
			p = skip_quoted(p);
		} else if (p[0] == '&' && p[1] == '&') {
			*p = '\0';
			trim_tail(start, p - 1);
			*p_out = p + 2;
			return start;
		} else {
			p++;
		}
	}

	char *end = p + strlen(p) - 1;
	trim_tail(start, end);
	*p_out = p;
	return start;
}

static bool has_chain_sep(const char *str)
{
	const char *p = str;

	while (*p) {
		if (*p == '\'' || *p == '"') {
			char quote = *p++;
			while (*p && *p != quote)
				p++;
			if (*p == quote)
				p++;
		} else if (p[0] == '&' && p[1] == '&') {
			return true;
		} else {
			p++;
		}
	}
	return false;
}

static void scheduler_cleanup(void)
{
	if (cmd_ctx.chain_buf) {
		cli_mpool_free(cmd_ctx.chain_buf);
		cmd_ctx.chain_buf = NULL;
		cmd_ctx.chain_p = NULL;
	}
	if (cmd_ctx.sub_chain_buf) {
		cli_mpool_free(cmd_ctx.sub_chain_buf);
		cmd_ctx.sub_chain_buf = NULL;
		cmd_ctx.sub_chain_p = NULL;
	}
}

static int dispatch_cmd(char *cmd)
{
	int status = cmd_parse_prepare(cmd, &cmd_ctx.cmd_def, &cmd_ctx.cmd_ret);
	if (status < 0)
		return -1;
	if (status == dispose_exit)
		return 0;

	strncpy(cmd_ctx.next_state, "scheduler_dispose",
		sizeof(cmd_ctx.next_state));
	return state_switch(&scheduler_eng, "scheduler_cmd_run");
}

int scheduler_dispose_task(void *arg)
{
	(void)arg;
	char *current_cmd = NULL;
	char *env_buf = NULL;
	int env_ret;

	if (!cmd_ctx.chain_buf) {
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
		cmd_ctx.chain_p = cmd_ctx.chain_buf;
		cmd_ctx.cmd_ret = 0;
	}

	if (cmd_ctx.cmd_ret < 0) {
		scheduler_cleanup();
		return state_switch(&scheduler_eng, "scheduler_get_char");
	}

	if (cmd_ctx.sub_chain_p) {
		current_cmd = extract_next_cmd(&cmd_ctx.sub_chain_p);
		if (!current_cmd) {
			cli_mpool_free(cmd_ctx.sub_chain_buf);
			cmd_ctx.sub_chain_buf = NULL;
			cmd_ctx.sub_chain_p = NULL;
		}
	}

	if (!current_cmd) {
		current_cmd = extract_next_cmd(&cmd_ctx.chain_p);
		if (!current_cmd) {
			scheduler_cleanup();
			return state_switch(&scheduler_eng,
					    "scheduler_get_char");
		}
	}

	env_buf = cli_mpool_alloc();
	if (!env_buf) {
		pr_err("out of memory\r\n");
		goto fail;
	}

	env_ret = cli_env_replace(current_cmd, env_buf, CLI_MPOOL_SIZE);
	if (env_ret == CLI_OK && env_buf[0] && has_chain_sep(env_buf)) {
		cmd_ctx.sub_chain_buf = env_buf;
		cmd_ctx.sub_chain_p = cmd_ctx.sub_chain_buf;
		return CLI_OK;
	}

	if (env_ret == CLI_OK && env_buf[0]) {
		current_cmd = env_buf;
	} else {
		cli_mpool_free(env_buf);
		env_buf = NULL;
	}

	if (dispatch_cmd(current_cmd) < 0) {
		if (env_buf)
			cli_mpool_free(env_buf);
		goto fail;
	}

	cmd_ctx.env_buf = env_buf;
	return CLI_OK;

fail:
	scheduler_cleanup();
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

int scheduler_is_in_get_char(void)
{
	return (scheduler_eng.from &&
		strcmp(scheduler_eng.from->name, "scheduler_get_char") == 0);
}

#if INLINE_TEST_EN
int cnt;
#endif
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
#if INLINE_TEST_EN
	cnt++;
	if ((cnt % 50) == 0)
		pr_info("test : %7d\r\n", cnt);
#endif
	return 0;
}
