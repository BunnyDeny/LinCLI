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

#include "cmd_dispose.h"
#include "stateM.h"
#include "cli_io.h"
#include "cli_cmd_line.h"
#include <stdlib.h>
#include <ctype.h>

struct tStateEngine dispose_mec;
extern struct tState * const _dispose_start[];
extern struct tState * const _dispose_end[];

char g_cli_cmd_buf[CLI_CMD_BUF_SIZE];
int g_last_cmd_ret = 0;

#define CLI_HELP_REQ_MARK_SIZE 16
#define CLI_HELP_DEP_MARK_SIZE 32
#define CLI_MAX_ARGV 64

static char cli_help_req_mark[CLI_HELP_REQ_MARK_SIZE];
static char cli_help_dep_mark[CLI_HELP_DEP_MARK_SIZE];

/**
 * @brief 在链接脚本段中按名称查找已注册的命令。
 */
static const cli_command_t *cli_command_find(const char *_name)
{
	const cli_command_t *_cmd;
	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, _cmd)
	{
		if (_cmd->name && strcmp(_cmd->name, _name) == 0)
			return _cmd;
	}
	return NULL;
}

/**
 * @brief 将一行输入字符串按空白字符切分为 argc/argv 形式。
 */
static int tokenize(char *line, char **argv, int max_argv)
{
	int argc = 0;
	char *p = line;

	while (*p && argc < max_argv) {
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		argv[argc++] = p;
		while (*p && *p != ' ' && *p != '\t')
			p++;
		if (*p)
			*p++ = '\0';
	}
	return argc;
}

/**
 * @brief 根据用户输入的选项字符串查找对应的 cli_option_t 定义。
 */
static const cli_option_t *find_option(const cli_command_t *cmd,
				       const char *arg)
{
	if (!arg || arg[0] != '-')
		return NULL;

	if (arg[1] == '-') {
		const char *name = arg + 2;
		for (size_t i = 0; i < cmd->option_count; i++) {
			if (cmd->options[i].long_opt &&
			    strcmp(cmd->options[i].long_opt, name) == 0)
				return &cmd->options[i];
		}
	} else {
		if (arg[2] != '\0')
			return NULL;
		char c = arg[1];
		for (size_t i = 0; i < cmd->option_count; i++) {
			if (cmd->options[i].short_opt &&
			    cmd->options[i].short_opt == c)
				return &cmd->options[i];
		}
	}
	return NULL;
}

/* ============================================================
 *  cli_auto_parse 拆分出的辅助函数
 * ============================================================ */

struct parse_state {
	const cli_option_t *cur_opt;
	int cur_opt_argc;
	int cur_opt_idx;
	char *scratch_pool;
	size_t scratch_remain;
};

static int check_prev_opt_missing_arg(struct parse_state *state)
{
	if (state->cur_opt && state->cur_opt->type != CLI_TYPE_BOOL &&
	    state->cur_opt_argc == 0) {
		pr_err("选项 -%c/--%s 缺少参数\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return -1;
	}
	return 0;
}

static int parse_option_switch(const cli_command_t *cmd, const char *arg,
			       void *arg_struct, bool *opt_seen,
			       struct parse_state *state)
{
	if (check_prev_opt_missing_arg(state) < 0)
		return -1;

	state->cur_opt = find_option(cmd, arg);
	if (!state->cur_opt) {
		pr_err("未知选项: %s\n", arg);
		return -1;
	}

	size_t idx = (size_t)(state->cur_opt - cmd->options);
	if (opt_seen[idx]) {
		pr_err("重复选项: -%c/--%s\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return -1;
	}
	opt_seen[idx] = true;
	state->cur_opt_argc = 0;
	state->cur_opt_idx = 0;

	if (state->cur_opt->type == CLI_TYPE_BOOL) {
		void *dst = (char *)arg_struct + state->cur_opt->offset;
		*(bool *)dst = true;
		state->cur_opt = NULL;
	}
	return 0;
}

static int parse_int_array(const cli_option_t *opt, const char *arg,
			   void *arg_struct, struct parse_state *state)
{
	if (state->cur_opt_idx >= (int)opt->max_args) {
		pr_err("选项 -%c/--%s 参数过多\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "");
		return -1;
	}

	void *dst = (char *)arg_struct + opt->offset;
	int *arr = *(int **)dst;
	if (!arr) {
		size_t need = opt->max_args * sizeof(int);
		if (need > state->scratch_remain) {
			pr_err("选项 -%c/--%s 缓冲区不足\n",
			       opt->short_opt ? opt->short_opt : ' ',
			       opt->long_opt ? opt->long_opt : "");
			return -1;
		}
		arr = (int *)state->scratch_pool;
		state->scratch_pool += need;
		state->scratch_remain -= need;
		*(int **)dst = arr;
	}
	arr[state->cur_opt_idx++] = atoi(arg);
	state->cur_opt_argc++;

	if (opt->offset_count > 0) {
		size_t *cnt =
			(size_t *)((char *)arg_struct + opt->offset_count);
		*cnt = (size_t)state->cur_opt_idx;
	}
	return 0;
}

static int parse_option_value(const char *arg, void *arg_struct,
			      struct parse_state *state)
{
	if (!state->cur_opt) {
		pr_err("孤立参数: %s\n", arg);
		return -1;
	}

	void *dst = (char *)arg_struct + state->cur_opt->offset;

	switch (state->cur_opt->type) {
	case CLI_TYPE_STRING:
	case CLI_TYPE_CALLBACK:
		*(const char **)dst = arg;
		state->cur_opt = NULL;
		break;
	case CLI_TYPE_INT:
		*(int *)dst = atoi(arg);
		state->cur_opt = NULL;
		break;
	case CLI_TYPE_DOUBLE:
		*(double *)dst = atof(arg);
		state->cur_opt = NULL;
		break;
	case CLI_TYPE_INT_ARRAY:
		if (parse_int_array(state->cur_opt, arg, arg_struct, state) < 0)
			return -1;
		break;
	default:
		pr_err("选项 -%c/--%s 类型未实现\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return -1;
	}
	return 0;
}

static int validate_end_state(struct parse_state *state)
{
	if (state->cur_opt && state->cur_opt->type != CLI_TYPE_BOOL &&
	    state->cur_opt_argc == 0) {
		pr_err("选项 -%c/--%s 缺少参数\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return -1;
	}
	return 0;
}

static int validate_required(const cli_command_t *cmd, const bool *opt_seen)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].required && !opt_seen[i]) {
			pr_err("缺少必需选项: -%c/--%s\n",
			       cmd->options[i].short_opt ?
				       cmd->options[i].short_opt :
				       ' ',
			       cmd->options[i].long_opt ?
				       cmd->options[i].long_opt :
				       "");
			return -1;
		}
	}
	return 0;
}

static bool find_target_opt(const cli_command_t *cmd, const bool *opt_seen,
			    const char *target_name)
{
	for (size_t j = 0; j < cmd->option_count; j++) {
		if (!opt_seen[j])
			continue;
		if (cmd->options[j].long_opt &&
		    strcmp(cmd->options[j].long_opt, target_name) == 0)
			return true;
		if (cmd->options[j].short_opt &&
		    target_name[0] == cmd->options[j].short_opt &&
		    target_name[1] == '\0')
			return true;
	}
	return false;
}

static int validate_depends_and_conflicts(const cli_command_t *cmd,
					  const bool *opt_seen)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (!opt_seen[i] || !cmd->options[i].depends)
			continue;

		const char *dep_str = cmd->options[i].depends;
		bool is_conflict = (dep_str[0] == '!');
		const char *target_name = is_conflict ? (dep_str + 1) : dep_str;
		bool target_found = find_target_opt(cmd, opt_seen, target_name);

		if (is_conflict) {
			if (target_found) {
				pr_err("选项 -%c/--%s 与 %s 互斥，不能同时使用\n",
				       cmd->options[i].short_opt ?
					       cmd->options[i].short_opt :
					       ' ',
				       cmd->options[i].long_opt ?
					       cmd->options[i].long_opt :
					       "",
				       target_name);
				return -1;
			}
		} else {
			if (!target_found) {
				pr_err("选项 -%c/--%s 依赖 %s 但未提供\n",
				       cmd->options[i].short_opt ?
					       cmd->options[i].short_opt :
					       ' ',
				       cmd->options[i].long_opt ?
					       cmd->options[i].long_opt :
					       "",
				       target_name);
				return -1;
			}
		}
	}
	return 0;
}

static int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv)
{
	if (!cmd || !argv || argc < 1)
		return -1;

	void *arg_struct = cmd->arg_buf;
	if (!arg_struct)
		return -1;

	memset(arg_struct, 0, cmd->arg_struct_size);

	char *scratch = (char *)arg_struct + cmd->arg_struct_size;
	size_t scratch_size =
		cmd->arg_buf_size > cmd->arg_struct_size ?
			(cmd->arg_buf_size - cmd->arg_struct_size) :
			0;

	size_t opt_seen_need = cmd->option_count * sizeof(bool);
	if (scratch_size < opt_seen_need) {
		pr_err("命令 %s 的参数缓冲区不足以容纳解析状态\n", cmd->name);
		return -1;
	}
	bool *opt_seen = (bool *)scratch;
	memset(opt_seen, 0, opt_seen_need);
	scratch += opt_seen_need;
	scratch_size -= opt_seen_need;

	struct parse_state state = { 0 };
	state.scratch_pool = scratch;
	state.scratch_remain = scratch_size;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (parse_option_switch(cmd, argv[i], arg_struct,
						opt_seen, &state) < 0) {
				return -1;
			}
		} else {
			const char *val_arg = argv[i];
			if (state.cur_opt &&
			    (state.cur_opt->type == CLI_TYPE_STRING ||
			     state.cur_opt->type == CLI_TYPE_CALLBACK)) {
				int end = i;
				size_t total = strlen(argv[i]);
				while (end + 1 < argc &&
				       argv[end + 1][0] != '-') {
					end++;
					total += 1 + strlen(argv[end]);
				}
				if (end > i) {
					if (total + 1 > state.scratch_remain) {
						pr_err("字符串参数过长\n");
						return -1;
					}
					char *dest = state.scratch_pool;
					size_t pos = 0;
					for (int j = i; j <= end; j++) {
						if (j > i)
							dest[pos++] = ' ';
						size_t len = strlen(argv[j]);
						memcpy(dest + pos, argv[j],
						       len);
						pos += len;
					}
					dest[pos] = '\0';
					state.scratch_pool += total + 1;
					state.scratch_remain -= total + 1;
					val_arg = dest;
					i = end;
				}
			}
			if (parse_option_value(val_arg, arg_struct, &state) <
			    0) {
				return -1;
			}
		}
	}

	if (validate_end_state(&state) < 0 ||
	    validate_required(cmd, opt_seen) < 0 ||
	    validate_depends_and_conflicts(cmd, opt_seen) < 0) {
		return -1;
	}

	return 0;
}

/**
 * @brief 打印指定命令的帮助信息。
 */
static void cli_print_help(const cli_command_t *cmd)
{
	if (!cmd)
		return;
	cli_printk("命令: %s\n", cmd->name);
	cli_printk("描述: %s\n", cmd->doc);
	cli_printk("选项:\n");
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		cli_help_req_mark[0] = '\0';
		cli_help_dep_mark[0] = '\0';
		if (opt->required)
			snprintf(cli_help_req_mark, CLI_HELP_REQ_MARK_SIZE,
				 " [必需]");
		if (opt->depends) {
			if (opt->depends[0] == '!')
				snprintf(cli_help_dep_mark,
					 CLI_HELP_DEP_MARK_SIZE, " [互斥:%s]",
					 opt->depends + 1);
			else
				snprintf(cli_help_dep_mark,
					 CLI_HELP_DEP_MARK_SIZE, " [依赖:%s]",
					 opt->depends);
		}
		cli_printk("  -%c, --%-16s %s%s%s\n",
			   opt->short_opt ? opt->short_opt : ' ',
			   opt->long_opt ? opt->long_opt : "",
			   opt->help ? opt->help : "", cli_help_req_mark,
			   cli_help_dep_mark);
	}
}

/**
 * @brief 检查命令行参数中是否包含帮助请求标志。
 */
static bool has_help_flag(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0)
			return true;
	}
	return false;
}

/**
 * @brief dispose 状态机的起始任务，完成完整的命令分派闭环。
 */
static int dispose_start_task(void *cmd)
{
	char *argv[CLI_MAX_ARGV];
	int argc = tokenize((char *)cmd, argv, CLI_MAX_ARGV);
	cli_printk("\r\n");
	if (argc < 1) {
		g_last_cmd_ret = 0;
		return dispose_exit;
	}

	const cli_command_t *cmd_def = cli_command_find(argv[0]);
	if (!cmd_def) {
		pr_err("未知命令: %s\n", argv[0]);
		g_last_cmd_ret = -1;
		return dispose_exit;
	}

	if (has_help_flag(argc, argv)) {
		cli_print_help(cmd_def);
		g_last_cmd_ret = 0;
		return dispose_exit;
	}

	if (!cmd_def->arg_buf ||
	    cmd_def->arg_struct_size > cmd_def->arg_buf_size) {
		pr_err("命令 %s 的参数缓冲区不足 (%zu > %zu)\n", cmd_def->name,
		       cmd_def->arg_struct_size,
		       cmd_def->arg_buf ? cmd_def->arg_buf_size : 0);
		g_last_cmd_ret = -1;
		return dispose_exit;
	}

	memset(cmd_def->arg_buf, 0, cmd_def->arg_buf_size);

	int status = cli_auto_parse(cmd_def, argc, argv);
	if (status < 0) {
		pr_err("命令解析失败: %s\n", argv[0]);
		cli_print_help(cmd_def);
		g_last_cmd_ret = -1;
		return dispose_exit;
	}

	if (cmd_def->validator) {
		int ret = cmd_def->validator(cmd_def->arg_buf);
		g_last_cmd_ret = ret;
		if (ret < 0) {
			pr_err("命令 %s 执行失败，返回值: %d\n", cmd_def->name,
			       ret);
			return dispose_exit;
		}
	} else {
		g_last_cmd_ret = 0;
	}

	return dispose_exit;
}
_EXPORT_STATE_SYMBOL(dispose_start, NULL, dispose_start_task, NULL, ".dispose");

/**
 * @brief 初始化 dispose 状态机引擎。
 */
int dispose_init(void)
{
	int status = engine_init(&dispose_mec, "dispose_start", _dispose_start,
				 _dispose_end);
	if (status < 0) {
		return status;
	}
	return 0;
}

/* ============================================================
 *  命令链（&&）支持
 * ============================================================ */

#define CMD_CHAIN_MAX 8

static int split_cmd_chain(char *buf, char **cmds, int max_cmds)
{
	int cnt = 0;
	char *p = buf;

	while (*p && cnt < max_cmds) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;

		cmds[cnt++] = p;

		char *next = strstr(p, "&&");
		if (next) {
			*next = '\0';
			char *end = next - 1;
			while (end > p && *end == ' ')
				*end-- = '\0';
			p = next + 2;
		} else {
			char *end = p + strlen(p) - 1;
			while (end > p && *end == ' ')
				*end-- = '\0';
			break;
		}
	}
	return cnt;
}

static int run_dispose_once(char *cmd)
{
	int status = dispose_init();
	if (status < 0) {
		pr_err("dispose_init异常\n");
		return -1;
	}

	g_last_cmd_ret = 0;
	while (1) {
		status = stateEngineRun(&dispose_mec, cmd);
		if (status < 0) {
			pr_err("dispose状态机异常\n");
			return -1;
		} else if (status == dispose_exit) {
			return 0;
		}
	}
}

/**
 * @brief 运行 dispose 状态机，支持 && 命令链。
 */
int dispose_task(char *cmd)
{
	if (!cmd || !cmd[0]) {
		g_last_cmd_ret = 0;
		return dispose_exit;
	}

	char chain_buf[CMD_LINE_BUF_SIZE];
	int len = strlen(cmd);
	if (len >= CMD_LINE_BUF_SIZE)
		len = CMD_LINE_BUF_SIZE - 1;
	memcpy(chain_buf, cmd, len);
	chain_buf[len] = '\0';

	char *cmds[CMD_CHAIN_MAX];
	int cnt = split_cmd_chain(chain_buf, cmds, CMD_CHAIN_MAX);

	for (int i = 0; i < cnt; i++) {
		if (!cmds[i] || cmds[i][0] == '\0') {
			pr_err("空命令\n");
			return dispose_exit;
		}

		int status = run_dispose_once(cmds[i]);
		if (status < 0)
			return status;

		if (g_last_cmd_ret < 0)
			return dispose_exit;
	}

	return dispose_exit;
}
