/*
 * LinCLI - A lightweight C command-line interaction framework for embedded/MCU.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
#include "cli_user.h"

#ifdef WORKQUEUE
#include "workqueue.h"
#endif

struct tStateEngine scheduler_eng;
extern struct tState *const _scheduler_start[];
extern struct tState *const _scheduler_end[];

static int scheduler_ticks = 0;
volatile unsigned long jiffies = 0;

__attribute__((weak)) void cli_prompt_print(void)
{
	all_printk(COLOR_BOLD COLOR_GREEN
		   "%s" COLOR_NONE COLOR_BOLD
		   "@" COLOR_NONE COLOR_BOLD COLOR_CYAN
		   "linCli" COLOR_NONE COLOR_BOLD COLOR_YELLOW "> " COLOR_NONE,
		   current_user->username);
}

void start_entry(void *private)
{
	CALL_INIT_D;
}
int start_task(void *private)
{
#if CLI_ENABLE_AUTO_RUN
	int status = state_switch(&scheduler_eng, STATE_ID_scheduler_auto_run);
#else
	int status = state_switch(&scheduler_eng, STATE_ID_scheduler_get_char);
#endif
	if (status < 0) {
		pr_crit("[scheduler] failed to switch \
			auto-run task, error code: %d\r\n",
			status);
		return status;
	}
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(scheduler_start, STATE_ID_scheduler_start, start_entry,
		     start_task, NULL, ".scheduler");

void scheduler_get_char_entry(void *private)
{
	int status = cli_cmd_line_init();
	if (status < 0) {
		pr_emerg("cli_cmd_line_init exception\r\n");
	}
	status = cli_in_clear();
	if (status < 0) {
		pr_err("clear input err\r\n");
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
		if (status == 0) {
			return CLI_ERR_FIFO_EMPTY;
		}
		status = cli_cmd_line_task(ch);
		if (status < 0) {
			return status;
		} else if (status == cmd_line_enter_press) {
			status = state_switch(&scheduler_eng,
					      STATE_ID_scheduler_dispose);
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
_EXPORT_STATE_SYMBOL(scheduler_get_char, STATE_ID_scheduler_get_char,
		     scheduler_get_char_entry, scheduler_get_char_task,
		     scheduler_get_char_exit, ".scheduler");

/* ============================================================
 * 命令执行上下文与新增 scheduler_cmd_run 状态
 * ============================================================ */

struct scheduler_cmd_ctx {
	const cli_command_t *cmd_def;
	int cmd_ret;
	cmd_parse_ctx_t parse_ctx;

#if CLI_ENABLE_CMD_CHAIN
	/* 主命令链 */
	char *chain_buf;
	char *chain_p;

	/* 子命令链（环境变量替换产生） */
	char *sub_chain_buf;
	char *sub_chain_p;
#endif

#if CLI_ENABLE_AUTO_RUN
	/* 自启动索引 */
	int auto_run_idx;
#endif

	/* 命令执行完后切换到的目标状态 */
	int next_state_id;

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
		return state_switch(&scheduler_eng, cmd_ctx.next_state_id);
	}

	int ret = cmd_ctx.cmd_def->cmd_task(cmd_ctx.cmd_def->arg_buf);
	cmd_ctx.cmd_ret = ret;

	bool is_legacy = (cmd_ctx.cmd_def->cmd_entry == NULL &&
			  cmd_ctx.cmd_def->cmd_exit == NULL);

	if (is_legacy) {
		return state_switch(&scheduler_eng, cmd_ctx.next_state_id);
	}

	if (ret == CLI_CONTINUE) {
		return CLI_OK;
	}

	return state_switch(&scheduler_eng, cmd_ctx.next_state_id);
}

void scheduler_cmd_run_exit(void *private)
{
	(void)private;
	if (cmd_ctx.cmd_def && cmd_ctx.cmd_def->cmd_exit) {
		cmd_ctx.cmd_def->cmd_exit(cmd_ctx.cmd_def->arg_buf);
	}

	if (cmd_ctx.cmd_ret < 0 && cmd_ctx.cmd_def) {
		pr_err("cmd '%s' failed: %d\r\n", cmd_ctx.cmd_def->name,
		       cmd_ctx.cmd_ret);
	}

	cmd_parse_cleanup(cmd_ctx.cmd_def, &cmd_ctx.parse_ctx);
	cmd_ctx.cmd_def = NULL;

	if (cmd_ctx.env_buf) {
		cli_mpool_free(cmd_ctx.env_buf);
		cmd_ctx.env_buf = NULL;
	}
}

_EXPORT_STATE_SYMBOL(scheduler_cmd_run, STATE_ID_scheduler_cmd_run,
		     scheduler_cmd_run_entry, scheduler_cmd_run_task,
		     scheduler_cmd_run_exit, ".scheduler");

/* ============================================================
 * 自启动命令状态（改造为异步状态驱动）
 * ============================================================ */

#if CLI_ENABLE_AUTO_RUN
static bool auto_run_should_exit(void)
{
	if (cmd_ctx.auto_run_idx > 0 && cmd_ctx.cmd_ret < 0) {
		cmd_ctx.auto_run_idx = 0;
		return true;
	}
	if (!cli_auto_cmds || cli_auto_cmds_count <= 0)
		return true;
	if (cmd_ctx.auto_run_idx >= cli_auto_cmds_count) {
		cmd_ctx.auto_run_idx = 0;
		return true;
	}
	return false;
}

static void auto_run_copy_cmd(const char *cmd)
{
	int len = strlen(cmd);
	if (len >= CMD_LINE_BUF_SIZE)
		len = CMD_LINE_BUF_SIZE - 1;
	memset(origin_cmd.buf, 0, CMD_LINE_BUF_SIZE);
	memcpy(origin_cmd.buf, cmd, len);
	origin_cmd.size = len;
}

static void auto_run_env_replace(void)
{
	char *env_buf = cli_mpool_alloc();
	if (!env_buf)
		return;
	int ret = cli_env_replace(origin_cmd.buf, env_buf, CLI_MPOOL_SIZE);
	if (ret == CLI_OK) {
		int env_len = strlen(env_buf);
		if (env_len < CMD_LINE_BUF_SIZE) {
			memcpy(origin_cmd.buf, env_buf, env_len + 1);
			origin_cmd.size = env_len;
		}
	}
	cli_mpool_free(env_buf);
}

static int auto_run_try_dispatch(void)
{
	int status = cmd_parse_prepare(origin_cmd.buf, &cmd_ctx.parse_ctx,
				       &cmd_ctx.cmd_def, &cmd_ctx.cmd_ret);
	if (status < 0 || status == dispose_exit) {
		cmd_ctx.auto_run_idx++;
		return CLI_OK;
	}
	cmd_ctx.auto_run_idx++;
	cmd_ctx.next_state_id = STATE_ID_scheduler_auto_run;
	return state_switch(&scheduler_eng, STATE_ID_scheduler_cmd_run);
}

int scheduler_auto_run_task(void *private)
{
	(void)private;
	if (auto_run_should_exit()) {
		cli_user_after_auto_run();
		return state_switch(&scheduler_eng,
				    STATE_ID_scheduler_get_char);
	}
	const char *cmd = cli_auto_cmds[cmd_ctx.auto_run_idx];
	if (!cmd) {
		cmd_ctx.auto_run_idx++;
		return CLI_OK;
	}
	auto_run_copy_cmd(cmd);
	auto_run_env_replace();
	return auto_run_try_dispatch();
}
_EXPORT_STATE_SYMBOL(scheduler_auto_run, STATE_ID_scheduler_auto_run, NULL,
		     scheduler_auto_run_task, NULL, ".scheduler");
#endif /* CLI_ENABLE_AUTO_RUN */

/* ============================================================
 * 命令分派状态（支持环境变量展开后的命令链）
 * ============================================================ */

extern int split_cmd_chain(char *buf, char **cmds, int max_cmds);

#if CLI_ENABLE_CMD_CHAIN
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
		if (*p == '\'' || *p == '"')
			p = skip_quoted(p);
		else if (p[0] == '&' && p[1] == '&') {
			*p = '\0';
			trim_tail(start, p - 1);
			*p_out = p + 2;
			return start;
		} else
			p++;
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
#endif /* CLI_ENABLE_CMD_CHAIN */

static void scheduler_cleanup(void)
{
#if CLI_ENABLE_CMD_CHAIN
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
#endif
	if (cmd_ctx.env_buf) {
		cli_mpool_free(cmd_ctx.env_buf);
		cmd_ctx.env_buf = NULL;
	}
}

static int dispatch_cmd(char *cmd)
{
	int status = cmd_parse_prepare(cmd, &cmd_ctx.parse_ctx,
				       &cmd_ctx.cmd_def, &cmd_ctx.cmd_ret);
	if (status < 0)
		return -1;
	if (status == dispose_exit)
		return 0;

	cmd_ctx.next_state_id = STATE_ID_scheduler_dispose;
	return state_switch(&scheduler_eng, STATE_ID_scheduler_cmd_run);
}

static bool should_dispose_exit(void)
{
#if CLI_ENABLE_CMD_CHAIN
	if (!cmd_ctx.chain_buf) {
		if (!origin_cmd.buf[0])
			return true;
		cmd_ctx.chain_buf = cli_mpool_alloc();
		if (!cmd_ctx.chain_buf) {
			pr_err("OOM\r\n");
			return true;
		}
		int len = origin_cmd.size;
		memcpy(cmd_ctx.chain_buf, origin_cmd.buf, len);
		cmd_ctx.chain_buf[len] = '\0';
		cmd_ctx.chain_p = cmd_ctx.chain_buf;
		cmd_ctx.cmd_ret = 0;
	}
#else
	if (!origin_cmd.buf[0])
		return true;
#endif
	if (cmd_ctx.cmd_ret < 0) {
		scheduler_cleanup();
		return true;
	}
	return false;
}

static char *extract_current_cmd(void)
{
#if CLI_ENABLE_CMD_CHAIN
	char *cmd = NULL;
	if (cmd_ctx.sub_chain_p) {
		cmd = extract_next_cmd(&cmd_ctx.sub_chain_p);
		if (!cmd) {
			cli_mpool_free(cmd_ctx.sub_chain_buf);
			cmd_ctx.sub_chain_buf = NULL;
			cmd_ctx.sub_chain_p = NULL;
		}
	}
	if (!cmd) {
		cmd = extract_next_cmd(&cmd_ctx.chain_p);
		if (!cmd) {
			scheduler_cleanup();
			return NULL;
		}
	}
	return cmd;
#else
	return origin_cmd.buf;
#endif
}

#if CLI_ENABLE_CMD_CHAIN
static bool try_sub_chain(char *env_buf)
{
	if (!has_chain_sep(env_buf))
		return false;
	if (cmd_ctx.sub_chain_buf) {
		cli_mpool_free(cmd_ctx.sub_chain_buf);
		cmd_ctx.sub_chain_buf = NULL;
		cmd_ctx.sub_chain_p = NULL;
	}
	cmd_ctx.sub_chain_buf = env_buf;
	cmd_ctx.sub_chain_p = cmd_ctx.sub_chain_buf;
	return true;
}
#endif /* CLI_ENABLE_CMD_CHAIN */

static int apply_env_replace(char *current_cmd, char **env_buf_out,
			     char **cmd_out)
{
	char *env_buf = cli_mpool_alloc();
	if (!env_buf) {
		pr_err("OOM\r\n");
		return -1;
	}
	int env_ret = cli_env_replace(current_cmd, env_buf, CLI_MPOOL_SIZE);
#if CLI_ENABLE_CMD_CHAIN
	if (env_ret == CLI_OK && env_buf[0] && try_sub_chain(env_buf))
		return 1;
#endif
	if (env_ret == CLI_OK && env_buf[0]) {
		*env_buf_out = env_buf;
		*cmd_out = env_buf;
		return 0;
	}
	cli_mpool_free(env_buf);
	return 0;
}

static int handle_dispatch_result(int dispatch_ret, char *env_buf)
{
	if (dispatch_ret < 0) {
		if (env_buf)
			cli_mpool_free(env_buf);
		return -1;
	}
	if (dispatch_ret == 0) {
		if (env_buf)
			cli_mpool_free(env_buf);
		return 0;
	}
	cmd_ctx.env_buf = env_buf;
	return 0;
}

static int dispose_fail(void)
{
	if (cmd_ctx.cmd_def) {
		cmd_parse_cleanup(cmd_ctx.cmd_def, &cmd_ctx.parse_ctx);
		cmd_ctx.cmd_def = NULL;
	}
	scheduler_cleanup();
	return state_switch(&scheduler_eng, STATE_ID_scheduler_get_char);
}

int scheduler_dispose_task(void *arg)
{
	(void)arg;
	char *env_buf = NULL;
	char *current_cmd;
	int ret;

	if (should_dispose_exit())
		return state_switch(&scheduler_eng,
				    STATE_ID_scheduler_get_char);
	current_cmd = extract_current_cmd();
	if (!current_cmd)
		return state_switch(&scheduler_eng,
				    STATE_ID_scheduler_get_char);
	ret = apply_env_replace(current_cmd, &env_buf, &current_cmd);
	if (ret < 0)
		return dispose_fail();
	if (ret == 1)
		return CLI_OK;
	ret = dispatch_cmd(current_cmd);
	if (handle_dispatch_result(ret, env_buf) < 0)
		return dispose_fail();
	return CLI_OK;
}
_EXPORT_STATE_SYMBOL(scheduler_dispose, STATE_ID_scheduler_dispose, NULL,
		     scheduler_dispose_task, NULL, ".scheduler");

int scheduler_init(void)
{
	int status;
	cli_io_init();
	status = engine_init(&scheduler_eng, STATE_ID_scheduler_start,
			     _scheduler_start, _scheduler_end);
	if (status < 0) {
		pr_emerg("[scheduler] init failed: %s (%d), \
			check scheduler state machine\r\n",
			 cli_strerror(status), status);
		return status;
	}
#ifdef WORKQUEUE
	workqueue_init();
#endif
	return 0;
}

int scheduler_is_in_get_char(void)
{
	return (scheduler_eng.from &&
		scheduler_eng.from->state_id == STATE_ID_scheduler_get_char);
}

#if CLI_ENABLE_SCHEDULER_TICK_PRINT
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
	++jiffies;
	scheduler_ticks = (int)jiffies;
#ifdef WORKQUEUE
	workqueue_tick_handler(system_wq, jiffies);
	workqueue_run_one(system_wq);
#endif
	if (cli_out_sync()) {
		return -2;
	}
#if CLI_ENABLE_SCHEDULER_TICK_PRINT
	cnt++;
	if ((cnt % 50) == 0)
		pr_info("test : %7d\r\n", cnt);
#endif
	return 0;
}

/* ============================================================
 *  Default system-info variables
 * ============================================================ */

#if CLI_ENABLE_VAR
CLI_VAR_RO(scheduler_ticks, "scheduler_ticks", INT, "tick");
#endif
