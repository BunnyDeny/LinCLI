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
#include "cli_errno.h"
#include "stateM.h"
#include "cli_io.h"
#include "cli_cmd_line.h"
#include "cli_mpool.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

struct tStateEngine dispose_mec;
extern struct tState *const _dispose_start[];
extern struct tState *const _dispose_end[];

struct dispose_ctx {
	char *cmd;
	int *cmd_ret;
};

#define CLI_HELP_REQ_MARK_SIZE 16
#define CLI_HELP_DEP_MARK_SIZE 128
#define CLI_MAX_ARGV 64

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
	const cli_command_t *cmd;
};

static int check_prev_opt_missing_arg(struct parse_state *state)
{
	if (state->cur_opt && state->cur_opt->type != CLI_TYPE_BOOL &&
	    state->cur_opt_argc == 0) {
		pr_err("option -%c/--%s missing argument\r\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return CLI_ERR_MISSING_ARG;
	}
	return 0;
}

static int parse_option_switch(const cli_command_t *cmd, const char *arg,
			       void *arg_struct, bool *opt_seen,
			       struct parse_state *state)
{
	int ret = check_prev_opt_missing_arg(state);
	if (ret < 0)
		return ret;

	state->cur_opt = find_option(cmd, arg);
	if (!state->cur_opt) {
		pr_err("unknow option: %s\r\n", arg);
		return CLI_ERR_UNKNOWN_OPT;
	}

	size_t idx = (size_t)(state->cur_opt - cmd->options);
	if (opt_seen[idx]) {
		pr_err("repeated option: -%c/--%s\r\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return CLI_ERR_DUP_OPT;
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

static int cli_parse_int(const char *arg, int *out)
{
	char *endptr;
	errno = 0;
	long val = strtol(arg, &endptr, 10);
	if (errno == ERANGE || val > INT_MAX || val < INT_MIN) {
		pr_err("'%s' out of integer range\r\n", arg);
		return CLI_ERR_INT_RANGE;
	}
	if (endptr == arg || *endptr != '\0') {
		pr_err("'%s' not a valid integer\r\n", arg);
		return CLI_ERR_INT_FMT;
	}
	*out = (int)val;
	return 0;
}

static int cli_parse_double(const char *arg, double *out)
{
	char *endptr;
	errno = 0;
	double val = strtod(arg, &endptr);
	if (errno == ERANGE) {
		pr_err("'%s' out of floating-point range\r\n", arg);
		return CLI_ERR_DOUBLE_RANGE;
	}
	if (endptr == arg || *endptr != '\0') {
		pr_err("'%s' not a valid floating-point number\r\n", arg);
		return CLI_ERR_DOUBLE_FMT;
	}
	*out = val;
	return 0;
}

static int parse_int_array(const cli_option_t *opt, const char *arg,
			   void *arg_struct, struct parse_state *state)
{
	int ret;
	if (state->cur_opt_idx >= (int)opt->max_args) {
		pr_err("option -%c/--%s too many arguments\r\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "");
		return CLI_ERR_ARRAY_MAX;
	}
	void *dst = (char *)arg_struct + opt->offset;
	int *arr = *(int **)dst;
	if (!arr) {
		size_t need = opt->max_args * sizeof(int);
		if (need > state->scratch_remain) {
			long shortfall =
				(long)need - (long)state->scratch_remain;
			pr_err("option -%c/--%s insufficient buffer, missing "
			       "%ld bytes (requires %zu b contiguous space)\r\n",
			       opt->short_opt ? opt->short_opt : ' ',
			       opt->long_opt ? opt->long_opt : "", shortfall,
			       need);
			return CLI_ERR_BUF_INSUFF;
		}
		arr = (int *)state->scratch_pool;
		state->scratch_pool += need;
		state->scratch_remain -= need;
		*(int **)dst = arr;
	}
	int val;
	ret = cli_parse_int(arg, &val);
	if (ret < 0)
		return ret;
	arr[state->cur_opt_idx++] = val;
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
		pr_err("orphaned argument: %s\r\n", arg);
		return CLI_ERR_ORPHAN_ARG;
	}
	void *dst = (char *)arg_struct + state->cur_opt->offset;
	int ret;

	switch (state->cur_opt->type) {
	case CLI_TYPE_STRING:
	case CLI_TYPE_CALLBACK:
		*(const char **)dst = arg;
		break;
	case CLI_TYPE_INT:
		ret = cli_parse_int(arg, (int *)dst);
		if (ret < 0)
			return ret;
		break;
	case CLI_TYPE_DOUBLE:
		ret = cli_parse_double(arg, (double *)dst);
		if (ret < 0)
			return ret;
		break;
	case CLI_TYPE_INT_ARRAY:
		ret = parse_int_array(state->cur_opt, arg, arg_struct, state);
		if (ret < 0)
			return ret;
		return CLI_OK;
	default:
		pr_err("option -%c/--%s type not implemented\r\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return CLI_ERR_INVAL;
	}
	state->cur_opt = NULL;
	return CLI_OK;
}

static int validate_end_state(struct parse_state *state)
{
	if (state->cur_opt && state->cur_opt->type != CLI_TYPE_BOOL &&
	    state->cur_opt_argc == 0) {
		pr_err("option -%c/--%s missing argument\r\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return CLI_ERR_MISSING_ARG;
	}
	return 0;
}

static int validate_required(const cli_command_t *cmd, const bool *opt_seen)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].required && !opt_seen[i]) {
			pr_err("missing required option: -%c/--%s\r\n",
			       cmd->options[i].short_opt ?
				       cmd->options[i].short_opt :
				       ' ',
			       cmd->options[i].long_opt ?
				       cmd->options[i].long_opt :
				       "");
			return CLI_ERR_REQ_OPT;
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

static int check_name_list(const cli_command_t *cmd, const bool *opt_seen,
			   size_t opt_idx, const char *list,
			   bool expect_present, int err_code)
{
	const cli_option_t *opt = &cmd->options[opt_idx];
	char name_buf[32];
	int idx;
	const char *p;

	p = list;
	while (*p) {
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p)
			break;
		idx = 0;
		while (*p && *p != ' ' && *p != '\t' &&
		       idx < (int)sizeof(name_buf) - 1) {
			name_buf[idx++] = *p++;
		}
		name_buf[idx] = '\0';
		bool found = find_target_opt(cmd, opt_seen, name_buf);
		if (expect_present && !found) {
			pr_err("option -%c/--%s depends on %s but not provided\r\n",
			       opt->short_opt ? opt->short_opt : ' ',
			       opt->long_opt ? opt->long_opt : "", name_buf);
			return err_code;
		}
		if (!expect_present && found) {
			pr_err("option -%c/--%s conflicts with %s, cannot be used together\r\n",
			       opt->short_opt ? opt->short_opt : ' ',
			       opt->long_opt ? opt->long_opt : "", name_buf);
			return err_code;
		}
	}
	return 0;
}

static int validate_depends(const cli_command_t *cmd, const bool *opt_seen)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (!opt_seen[i] || !cmd->options[i].depends)
			continue;
		int ret = check_name_list(cmd, opt_seen, i,
					  cmd->options[i].depends, true,
					  CLI_ERR_DEP_MISSING);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int validate_conflicts(const cli_command_t *cmd, const bool *opt_seen)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (!opt_seen[i] || !cmd->options[i].conflicts)
			continue;
		int ret = check_name_list(cmd, opt_seen, i,
					  cmd->options[i].conflicts, false,
					  CLI_ERR_CONFLICT);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int join_string_args(int argc, char **argv, int start,
			    struct parse_state *state, const char **out_arg,
			    int *consumed)
{
	int end = start;
	size_t total = strlen(argv[start]);
	while (end + 1 < argc && argv[end + 1][0] != '-') {
		end++;
		total += 1 + strlen(argv[end]);
	}
	if (end == start) {
		*out_arg = argv[start];
		*consumed = 0;
		return CLI_OK;
	}
	if (total + 1 > state->scratch_remain) {
		long shortfall =
			(long)(total + 1) - (long)state->scratch_remain;
		pr_err("string argument too long, missing "
		       "%ld bytes (%zu b required)\r\n",
		       shortfall, total + 1);
		return CLI_ERR_BUF_INSUFF;
	}
	char *dest = state->scratch_pool;
	size_t pos = 0;
	for (int j = start; j <= end; j++) {
		if (j > start)
			dest[pos++] = ' ';
		size_t len = strlen(argv[j]);
		memcpy(dest + pos, argv[j], len);
		pos += len;
	}
	dest[pos] = '\0';
	state->scratch_pool += total + 1;
	state->scratch_remain -= total + 1;
	*out_arg = dest;
	*consumed = end - start;
	return CLI_OK;
}

static int validate_parsed_result(const cli_command_t *cmd,
				  struct parse_state *state,
				  const bool *opt_seen)
{
	int ret;
	ret = validate_end_state(state);
	if (ret < 0)
		return ret;
	ret = validate_required(cmd, opt_seen);
	if (ret < 0)
		return ret;
	ret = validate_depends(cmd, opt_seen);
	if (ret < 0)
		return ret;
	ret = validate_conflicts(cmd, opt_seen);
	if (ret < 0)
		return ret;
	return CLI_OK;
}

static int parse_init(const cli_command_t *cmd, void **arg_struct_out,
		      bool **opt_seen_out, struct parse_state *state_out)
{
	void *arg_struct = cmd->arg_buf;
	if (!arg_struct)
		return CLI_ERR_NULL;
	memset(arg_struct, 0, cmd->arg_struct_size);

	char *scratch = (char *)arg_struct + cmd->arg_struct_size;
	long scratch_avail =
		(long)cmd->arg_buf_size - (long)cmd->arg_struct_size;
	size_t scratch_size = scratch_avail > 0 ? (size_t)scratch_avail : 0;

	size_t opt_seen_need = cmd->option_count * sizeof(bool);
	if (scratch_avail < (long)opt_seen_need) {
		pr_err("command %s insufficient buffer, "
		       "missing %ld bytes\r\n",
		       cmd->name, (long)opt_seen_need - scratch_avail);
		return CLI_ERR_BUF_INSUFF;
	}
	bool *opt_seen = (bool *)scratch;
	memset(opt_seen, 0, opt_seen_need);
	scratch += opt_seen_need;
	scratch_size -= opt_seen_need;

	struct parse_state state = { 0 };
	state.scratch_pool = scratch;
	state.scratch_remain = scratch_size;
	state.cmd = cmd;

	*arg_struct_out = arg_struct;
	*opt_seen_out = opt_seen;
	*state_out = state;
	return CLI_OK;
}

static int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv)
{
	int ret;
	void *arg_struct;
	bool *opt_seen;
	struct parse_state state;
	if (!cmd || !argv || argc < 1)
		return CLI_ERR_NULL;
	ret = parse_init(cmd, &arg_struct, &opt_seen, &state);
	if (ret < 0)
		return ret;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			ret = parse_option_switch(cmd, argv[i], arg_struct,
						  opt_seen, &state);
			if (ret < 0)
				return ret;
		} else {
			const char *val_arg = argv[i];
			if (state.cur_opt &&
			    (state.cur_opt->type == CLI_TYPE_STRING ||
			     state.cur_opt->type == CLI_TYPE_CALLBACK)) {
				int consumed = 0;
				ret = join_string_args(argc, argv, i, &state,
						       &val_arg, &consumed);
				if (ret < 0)
					return ret;
				i += consumed;
			}
			ret = parse_option_value(val_arg, arg_struct, &state);
			if (ret < 0)
				return ret;
		}
	}
	ret = validate_parsed_result(cmd, &state, opt_seen);
	if (ret < 0)
		return ret;
	return CLI_OK;
}

/**
 * @brief 打印指定命令的帮助信息。
 */
static void cli_print_help(const cli_command_t *cmd)
{
	char *cli_help_req_mark = NULL;
	char *cli_help_dep_mark = NULL;

	if (!cmd)
		return;

	cli_help_req_mark = cli_mpool_alloc();
	cli_help_dep_mark = cli_mpool_alloc();
	if (!cli_help_req_mark || !cli_help_dep_mark) {
		if (cli_help_req_mark)
			cli_mpool_free(cli_help_req_mark);
		if (cli_help_dep_mark)
			cli_mpool_free(cli_help_dep_mark);
		return;
	}

	cli_printk(" command     : %s\r\n", cmd->name);
	cli_printk(" description : %s\r\n", cmd->doc);
	cli_printk(" option      :\r\n");
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		cli_help_req_mark[0] = '\0';
		cli_help_dep_mark[0] = '\0';
		if (opt->required)
			snprintf(cli_help_req_mark, CLI_HELP_REQ_MARK_SIZE,
				 " [required]");
		if (opt->depends && opt->depends[0]) {
			snprintf(cli_help_dep_mark, CLI_HELP_DEP_MARK_SIZE,
				 " [depends:%s]", opt->depends);
		}
		if (opt->conflicts && opt->conflicts[0]) {
			size_t len = strlen(cli_help_dep_mark);
			if (len < CLI_HELP_DEP_MARK_SIZE - 1) {
				snprintf(cli_help_dep_mark + len,
					 CLI_HELP_DEP_MARK_SIZE - len,
					 " [conflicts:%s]", opt->conflicts);
			}
		}
		cli_printk("      -%c, --%-16s %s%s%s\r\n",
			   opt->short_opt ? opt->short_opt : ' ',
			   opt->long_opt ? opt->long_opt : "",
			   opt->help ? opt->help : "", cli_help_req_mark,
			   cli_help_dep_mark);
	}
	cli_mpool_free(cli_help_req_mark);
	cli_mpool_free(cli_help_dep_mark);
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
static cli_command_t cmd_runtime;

static const cli_command_t *prepare_cmd_def(int argc, char **argv, int *cmd_ret)
{
	if (argc < 1) {
		*cmd_ret = 0;
		return NULL;
	}
	const cli_command_t *cmd_def = cli_command_find(argv[0]);
	if (!cmd_def) {
		pr_err("unknown command: %s\r\n", argv[0]);
		*cmd_ret = -1;
		return NULL;
	}
	if (has_help_flag(argc, argv)) {
		cli_print_help(cmd_def);
		*cmd_ret = 0;
		return NULL;
	}
	if (!cmd_def->arg_buf) {
		/* CLI_COMMAND 注册的命令：从内存池动态申请缓冲区 */
		memcpy(&cmd_runtime, cmd_def, sizeof(cli_command_t));
		cmd_runtime.arg_buf = cli_mpool_alloc();
		if (!cmd_runtime.arg_buf) {
			pr_err("command %s out of memory\r\n", cmd_def->name);
			*cmd_ret = -1;
			return NULL;
		}
		cmd_def = &cmd_runtime;
	}
	if (cmd_def->arg_struct_size > cmd_def->arg_buf_size) {
		pr_err("command %s struct size %zu bytes, "
		       "exceeds buffer by %zu bytes\r\n",
		       cmd_def->name, cmd_def->arg_struct_size,
		       cmd_def->arg_buf_size);
		*cmd_ret = -1;
		return NULL;
	}
	return cmd_def;
}

static int run_cmd_validator(const cli_command_t *cmd_def, int *cmd_ret)
{
	if (!cmd_def->validator) {
		*cmd_ret = 0;
		return 0;
	}
	if (cli_in_clear() < 0) {
		pr_err("failed to clear input buffer\r\n");
		*cmd_ret = -1;
		return -1;
	}
	int ret = cmd_def->validator(cmd_def->arg_buf);
	*cmd_ret = ret;
	if (ret < 0) {
		pr_err("command %s execution failed, return value: %d\r\n",
		       cmd_def->name, ret);
		return -1;
	}
	return 0;
}

static int dispose_start_task(void *arg)
{
	static char *argv[CLI_MAX_ARGV];
	struct dispose_ctx *ctx = (struct dispose_ctx *)arg;
	int argc = tokenize(ctx->cmd, argv, CLI_MAX_ARGV);
	cli_printk("\r\n");
	cli_printk("\033[K");

	const cli_command_t *cmd_def =
		prepare_cmd_def(argc, argv, ctx->cmd_ret);
	if (!cmd_def)
		return dispose_exit;

	memset(cmd_def->arg_buf, 0, cmd_def->arg_buf_size);

	int status = cli_auto_parse(cmd_def, argc, argv);
	if (status < 0) {
		pr_err("command parsing failed: %s\r\n", argv[0]);
		cli_print_help(cmd_def);
		*ctx->cmd_ret = -1;
	} else {
		run_cmd_validator(cmd_def, ctx->cmd_ret);
	}

	if (cmd_def == &cmd_runtime && cmd_runtime.arg_buf) {
		cli_mpool_free(cmd_runtime.arg_buf);
		cmd_runtime.arg_buf = NULL;
	}
	return dispose_exit;
}
_EXPORT_STATE_SYMBOL(dispose_start, NULL, dispose_start_task, NULL, ".dispose");

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

#if ALIAS_EN
static int alias_handler(void *_args)
{
	struct alias_cmd *alias_cmd;
	cli_printk("\r\n%-20s %-40s\r\n", "ALIAS", "ORIGINAL COMMAND");
	cli_printk(
		"------------------------------------------------------------\r\n");
	FOR_EACH_ALIAS(_alias_cmd_start, _alias_cmd_end, alias_cmd)
	{
		cli_printk("%-20s %-40s\r\n", alias_cmd->alias_name,
			   alias_cmd->original_name);
	}
	cli_printk(
		"------------------------------------------------------------\r\n");
	return 0;
}

CLI_COMMAND_NO_STRUCT(alias, "alias", "list all the alias cmds", alias_handler);

static bool is_prefix(char *pre, char *str)
{
	int idx = 0;
	while (pre[idx]) {
		if (pre[idx] != str[idx]) {
			return false;
		}
		idx++;
	}
	return true;
}

static char *alias_replace(char *cmd, char *buf, size_t buf_size)
{
	struct alias_cmd *alias_cmd;
	char *p = cmd;
	while ((*p) && (*p) != ' ' && (*p) != '\t') {
		p++;
	}
	FOR_EACH_ALIAS(_alias_cmd_start, _alias_cmd_end, alias_cmd)
	{
		if (is_prefix(alias_cmd->alias_name, cmd)) {
			memset(buf, 0, buf_size);
			memcpy(buf, alias_cmd->original_name,
			       strlen(alias_cmd->original_name));
			if ((*p)) {
				strncat(buf, p, buf_size - strlen(buf) - 1);
			}
			return buf;
		}
	}
	return cmd;
}
#endif

static int run_dispose_once(char *cmd, int *cmd_ret)
{
	int status = dispose_init();
	if (status < 0) {
		pr_err("dispose_init exception: %s (%d)\r\n",
		       cli_strerror(status), status);
		return status;
	}
	char *alias_buf = cli_mpool_alloc();
	if (!alias_buf) {
		pr_err("out of memory\r\n");
		return CLI_ERR_NULL;
	}
#if ALIAS_EN
	cmd = alias_replace(cmd, alias_buf, CLI_MPOOL_SIZE);
#endif
	struct dispose_ctx ctx = { cmd, cmd_ret };
	*cmd_ret = 0;
	while (1) {
		status = stateEngineRun(&dispose_mec, &ctx);
		if (status < 0) {
			pr_err("dispose state machine exception, "
			       "error code: %d\r\n",
			       status);
			cli_mpool_free(alias_buf);
			return status;
		} else if (status == dispose_exit) {
			cli_mpool_free(alias_buf);
			return CLI_OK;
		}
	}
}

int dispose_init(void)
{
	int status = engine_init(&dispose_mec, "dispose_start", _dispose_start,
				 _dispose_end);
	if (status < 0) {
		return status;
	}
	return CLI_OK;
}

int dispose_task(char *cmd, int *cmd_ret)
{
	int _local_ret = 0;
	int ret = dispose_exit;
	char *chain_buf = NULL;
	int len;
	int cnt;
	char *cmds[CMD_CHAIN_MAX];

	if (!cmd_ret)
		cmd_ret = &_local_ret;
	if (!cmd || !cmd[0]) {
		*cmd_ret = 0;
		goto out;
	}

	chain_buf = cli_mpool_alloc();
	if (!chain_buf) {
		pr_err("out of memory\r\n");
		*cmd_ret = -1;
		goto out;
	}

	len = strlen(cmd);
	if (len >= CMD_LINE_BUF_SIZE)
		len = CMD_LINE_BUF_SIZE - 1;
	memcpy(chain_buf, cmd, len);
	chain_buf[len] = '\0';
	cnt = split_cmd_chain(chain_buf, cmds, CMD_CHAIN_MAX);
	for (int i = 0; i < cnt; i++) {
		if (!cmds[i] || cmds[i][0] == '\0') {
			pr_err("empty command\r\n");
			*cmd_ret = 0;
			goto out;
		}
		int status = run_dispose_once(cmds[i], cmd_ret);
		if (status < 0) {
			ret = status;
			goto out;
		}
		if (*cmd_ret < 0)
			goto out;
	}

out:
	if (chain_buf)
		cli_mpool_free(chain_buf);
	return ret;
}
