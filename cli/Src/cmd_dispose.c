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
static cli_option_t *find_option(const cli_command_t *cmd,
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
	cli_option_t *cur_opt;
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

static int resolve_option(const cli_command_t *cmd, const char *arg,
			  struct parse_state *state)
{
	state->cur_opt = find_option(cmd, arg);
	if (!state->cur_opt) {
		pr_err("unknow option: %s\r\n", arg);
		return CLI_ERR_UNKNOWN_OPT;
	}
	return 0;
}

static int mark_option_seen(cli_option_t *opt, bool *opt_seen,
			    struct parse_state *state)
{
	size_t idx = (size_t)(opt - state->cmd->options);
	if (opt_seen[idx]) {
		pr_err("repeated option: -%c/--%s\r\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "");
		return CLI_ERR_DUP_OPT;
	}
	opt_seen[idx] = true;
	state->cur_opt_argc = 0;
	state->cur_opt_idx = 0;
	return 0;
}

static void apply_bool_option(cli_option_t *opt, void *arg_struct)
{
	void *dst = (char *)arg_struct + opt->offset;
	*(bool *)dst = true;
}

static int parse_option_switch(const cli_command_t *cmd, const char *arg,
			       void *arg_struct, bool *opt_seen,
			       struct parse_state *state)
{
	int ret = check_prev_opt_missing_arg(state);
	if (ret < 0)
		return ret;
	ret = resolve_option(cmd, arg, state);
	if (ret < 0)
		return ret;
	ret = mark_option_seen(state->cur_opt, opt_seen, state);
	if (ret < 0)
		return ret;
	if (state->cur_opt->type == CLI_TYPE_BOOL) {
		apply_bool_option(state->cur_opt, arg_struct);
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

static int *ensure_int_array(cli_option_t *opt, void *arg_struct,
			     struct parse_state *state)
{
	void *dst = (char *)arg_struct + opt->offset;
	int *arr = *(int **)dst;
	if (arr)
		return arr;
	size_t need = opt->max_args * sizeof(int);
	if (need > state->scratch_remain) {
		long shortfall = (long)need - (long)state->scratch_remain;
		pr_err("option -%c/--%s insufficient buffer, missing "
		       "%ld bytes (requires %zu b contiguous space)\r\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "", shortfall, need);
		return NULL;
	}
	arr = (int *)state->scratch_pool;
	state->scratch_pool += need;
	state->scratch_remain -= need;
	*(int **)dst = arr;
	return arr;
}

static void update_array_count(cli_option_t *opt, void *arg_struct,
			       int cur_count)
{
	if (opt->offset_count > 0) {
		size_t *cnt =
			(size_t *)((char *)arg_struct + opt->offset_count);
		*cnt = (size_t)cur_count;
	}
}

static int parse_int_array(cli_option_t *opt, const char *arg,
			   void *arg_struct, struct parse_state *state)
{
	if (state->cur_opt_idx >= (int)opt->max_args) {
		pr_err("option -%c/--%s too many arguments\r\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "");
		return CLI_ERR_ARRAY_MAX;
	}
	int *arr = ensure_int_array(opt, arg_struct, state);
	if (!arr)
		return CLI_ERR_BUF_INSUFF;
	int val;
	int ret = cli_parse_int(arg, &val);
	if (ret < 0)
		return ret;
	arr[state->cur_opt_idx++] = val;
	state->cur_opt_argc++;
	update_array_count(opt, arg_struct, state->cur_opt_idx);
	return 0;
}

static int parse_value_by_type(const char *arg, void *arg_struct,
			       struct parse_state *state)
{
	void *dst = (char *)arg_struct + state->cur_opt->offset;
	switch (state->cur_opt->type) {
	case CLI_TYPE_STRING:
	case CLI_TYPE_CALLBACK:
		*(const char **)dst = arg;
		return 0;
	case CLI_TYPE_INT:
		return cli_parse_int(arg, (int *)dst);
	case CLI_TYPE_DOUBLE:
		return cli_parse_double(arg, (double *)dst);
	case CLI_TYPE_INT_ARRAY:
		return parse_int_array(state->cur_opt, arg, arg_struct, state);
	default:
		pr_err("option -%c/--%s type not implemented\r\n",
		       state->cur_opt->short_opt ? state->cur_opt->short_opt :
						   ' ',
		       state->cur_opt->long_opt ? state->cur_opt->long_opt :
						  "");
		return CLI_ERR_INVAL;
	}
}

static int parse_option_value(const char *arg, void *arg_struct,
			      struct parse_state *state)
{
	if (!state->cur_opt) {
		pr_err("orphaned argument: %s\r\n", arg);
		return CLI_ERR_ORPHAN_ARG;
	}
	int ret = parse_value_by_type(arg, arg_struct, state);
	if (ret < 0)
		return ret;
	if (state->cur_opt->type != CLI_TYPE_INT_ARRAY)
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

static bool extract_next_name(const char **p, char *buf, size_t buf_size)
{
	while (**p == ' ' || **p == '\t')
		(*p)++;
	if (!**p)
		return false;
	size_t idx = 0;
	while (**p && **p != ' ' && **p != '\t' && idx < buf_size - 1) {
		buf[idx++] = *(*p)++;
	}
	buf[idx] = '\0';
	return true;
}

static int report_name_check_error(cli_option_t *opt,
				   const char *name_buf, bool expect_present,
				   int err_code)
{
	if (expect_present) {
		pr_err("option -%c/--%s depends on %s but not provided\r\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "", name_buf);
	} else {
		pr_err("option -%c/--%s conflicts with %s, cannot be used together\r\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "", name_buf);
	}
	return err_code;
}

static int check_name_list(const cli_command_t *cmd, const bool *opt_seen,
			   size_t opt_idx, const char *list,
			   bool expect_present, int err_code)
{
	cli_option_t *opt = &cmd->options[opt_idx];
	char name_buf[32];
	const char *p = list;
	while (extract_next_name(&p, name_buf, sizeof(name_buf))) {
		bool found = find_target_opt(cmd, opt_seen, name_buf);
		if (found != expect_present)
			return report_name_check_error(
				opt, name_buf, expect_present, err_code);
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

static void find_string_arg_end(int argc, char **argv, int start,
				size_t *total_len, int *end)
{
	int e = start;
	size_t total = strlen(argv[start]);
	while (e + 1 < argc && argv[e + 1][0] != '-') {
		e++;
		total += 1 + strlen(argv[e]);
	}
	*total_len = total;
	*end = e;
}

static int alloc_scratch_string(size_t need, struct parse_state *state,
				char **dest)
{
	if (need > state->scratch_remain) {
		long shortfall = (long)need - (long)state->scratch_remain;
		pr_err("string argument too long, missing "
		       "%ld bytes (%zu b required)\r\n",
		       shortfall, need);
		return CLI_ERR_BUF_INSUFF;
	}
	*dest = state->scratch_pool;
	state->scratch_pool += need;
	state->scratch_remain -= need;
	return CLI_OK;
}

static void do_join_args(char **argv, int start, int end, char *dest)
{
	size_t pos = 0;
	for (int j = start; j <= end; j++) {
		if (j > start)
			dest[pos++] = ' ';
		size_t len = strlen(argv[j]);
		memcpy(dest + pos, argv[j], len);
		pos += len;
	}
	dest[pos] = '\0';
}

static int join_string_args(int argc, char **argv, int start,
			    struct parse_state *state, const char **out_arg,
			    int *consumed)
{
	size_t total;
	int end;
	find_string_arg_end(argc, argv, start, &total, &end);
	if (end == start) {
		*out_arg = argv[start];
		*consumed = 0;
		return CLI_OK;
	}
	char *dest;
	int ret = alloc_scratch_string(total + 1, state, &dest);
	if (ret < 0)
		return ret;
	do_join_args(argv, start, end, dest);
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

static int init_arg_struct(const cli_command_t *cmd, void **arg_struct_out,
			   char **scratch_out, size_t *scratch_size_out)
{
	void *arg_struct = cmd->arg_buf;
	if (!arg_struct)
		return CLI_ERR_NULL;
	memset(arg_struct, 0, cmd->arg_struct_size);
	long avail = (long)cmd->arg_buf_size - (long)cmd->arg_struct_size;
	*scratch_out = (char *)arg_struct + cmd->arg_struct_size;
	*scratch_size_out = avail > 0 ? (size_t)avail : 0;
	*arg_struct_out = arg_struct;
	return CLI_OK;
}

static int alloc_opt_seen(const cli_command_t *cmd, char **scratch,
			  size_t *scratch_size, bool **opt_seen_out)
{
	size_t need = cmd->option_count * sizeof(bool);
	long avail = (long)*scratch_size;
	if (avail < (long)need) {
		pr_err("command %s insufficient buffer, missing %ld bytes\r\n",
		       cmd->name, (long)need - avail);
		return CLI_ERR_BUF_INSUFF;
	}
	bool *opt_seen = (bool *)*scratch;
	memset(opt_seen, 0, need);
	*scratch += need;
	*scratch_size -= need;
	*opt_seen_out = opt_seen;
	return CLI_OK;
}

static void init_parse_state(const cli_command_t *cmd, char *scratch,
			     size_t scratch_size, struct parse_state *state_out)
{
	struct parse_state state = { 0 };
	state.scratch_pool = scratch;
	state.scratch_remain = scratch_size;
	state.cmd = cmd;
	*state_out = state;
}

static int parse_init(const cli_command_t *cmd, void **arg_struct_out,
		      bool **opt_seen_out, struct parse_state *state_out)
{
	char *scratch;
	size_t scratch_size;
	int ret = init_arg_struct(cmd, arg_struct_out, &scratch, &scratch_size);
	if (ret < 0)
		return ret;
	ret = alloc_opt_seen(cmd, &scratch, &scratch_size, opt_seen_out);
	if (ret < 0)
		return ret;
	init_parse_state(cmd, scratch, scratch_size, state_out);
	return CLI_OK;
}

static int handle_value_arg(int argc, char **argv, int *i, void *arg_struct,
			    struct parse_state *state)
{
	const char *val_arg = argv[*i];
	if (state->cur_opt && (state->cur_opt->type == CLI_TYPE_STRING ||
			       state->cur_opt->type == CLI_TYPE_CALLBACK)) {
		int consumed = 0;
		int ret = join_string_args(argc, argv, *i, state, &val_arg,
					   &consumed);
		if (ret < 0)
			return ret;
		*i += consumed;
	}
	return parse_option_value(val_arg, arg_struct, state);
}

static int parse_args_loop(const cli_command_t *cmd, int argc, char **argv,
			   void *arg_struct, bool *opt_seen,
			   struct parse_state *state)
{
	int ret;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			ret = parse_option_switch(cmd, argv[i], arg_struct,
						  opt_seen, state);
			if (ret < 0)
				return ret;
		} else {
			ret = handle_value_arg(argc, argv, &i, arg_struct,
					       state);
			if (ret < 0)
				return ret;
		}
	}
	return CLI_OK;
}

static int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv)
{
	void *arg_struct;
	bool *opt_seen;
	struct parse_state state;
	if (!cmd || !argv || argc < 1)
		return CLI_ERR_NULL;
	int ret = parse_init(cmd, &arg_struct, &opt_seen, &state);
	if (ret < 0)
		return ret;
	ret = parse_args_loop(cmd, argc, argv, arg_struct, opt_seen, &state);
	if (ret < 0)
		return ret;
	return validate_parsed_result(cmd, &state, opt_seen);
}

/**
 * @brief 打印指定命令的帮助信息。
 */
static void build_opt_marks(cli_option_t *opt, char *req_mark,
			    char *dep_mark, size_t dep_mark_size)
{
	req_mark[0] = '\0';
	dep_mark[0] = '\0';
	if (opt->required)
		snprintf(req_mark, CLI_HELP_REQ_MARK_SIZE, " [required]");
	if (opt->depends && opt->depends[0]) {
		snprintf(dep_mark, dep_mark_size, " [depends:%s]",
			 opt->depends);
	}
	if (opt->conflicts && opt->conflicts[0]) {
		size_t len = strlen(dep_mark);
		if (len < dep_mark_size - 1) {
			snprintf(dep_mark + len, dep_mark_size - len,
				 " [conflicts:%s]", opt->conflicts);
		}
	}
}

static bool alloc_help_marks(char **req_mark, char **dep_mark)
{
	*req_mark = cli_mpool_alloc();
	*dep_mark = cli_mpool_alloc();
	if (!*req_mark || !*dep_mark) {
		if (*req_mark)
			cli_mpool_free(*req_mark);
		if (*dep_mark)
			cli_mpool_free(*dep_mark);
		return false;
	}
	return true;
}

static void free_help_marks(char *req_mark, char *dep_mark)
{
	if (req_mark)
		cli_mpool_free(req_mark);
	if (dep_mark)
		cli_mpool_free(dep_mark);
}

static void print_cmd_header(const cli_command_t *cmd)
{
	all_printk(" command     : %s\r\n", cmd->name);
	all_printk(" description : %s\r\n", cmd->doc);
	all_printk(" option      :\r\n");
}

static void print_opt_line(cli_option_t *opt, const char *req_mark,
			   const char *dep_mark)
{
	all_printk("      -%c, --%-16s %s%s%s\r\n",
		   opt->short_opt ? opt->short_opt : ' ',
		   opt->long_opt ? opt->long_opt : "",
		   opt->help ? opt->help : "", req_mark, dep_mark);
}

static void cli_print_help(const cli_command_t *cmd)
{
	char *req_mark = NULL;
	char *dep_mark = NULL;
	if (!cmd)
		return;
	if (!alloc_help_marks(&req_mark, &dep_mark))
		return;

	print_cmd_header(cmd);
	for (size_t i = 0; i < cmd->option_count; i++) {
		cli_option_t *opt = &cmd->options[i];
		build_opt_marks(opt, req_mark, dep_mark,
				CLI_HELP_DEP_MARK_SIZE);
		print_opt_line(opt, req_mark, dep_mark);
	}
	free_help_marks(req_mark, dep_mark);
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

static const cli_command_t *lookup_cmd_def(char *cmd_name, int *cmd_ret)
{
	const cli_command_t *cmd_def = cli_command_find(cmd_name);
	if (!cmd_def) {
		pr_err("unknown command: %s\r\n", cmd_name);
		*cmd_ret = -1;
	}
	return cmd_def;
}

static bool handle_help_request(const cli_command_t *cmd_def, int argc,
				char **argv, int *cmd_ret)
{
	if (has_help_flag(argc, argv)) {
		cli_print_help(cmd_def);
		*cmd_ret = 0;
		return true;
	}
	return false;
}

static int ensure_cmd_buf(const cli_command_t **cmd_def_p)
{
	const cli_command_t *cmd_def = *cmd_def_p;
	if (cmd_def->arg_buf)
		return 0;
	memcpy(&cmd_runtime, cmd_def, sizeof(cli_command_t));
	cmd_runtime.arg_buf = cli_mpool_alloc();
	if (!cmd_runtime.arg_buf) {
		pr_err("command %s out of memory\r\n", cmd_def->name);
		return -1;
	}
	*cmd_def_p = &cmd_runtime;
	return 0;
}

static bool validate_cmd_buf_size(const cli_command_t *cmd_def, int *cmd_ret)
{
	if (cmd_def->arg_struct_size > cmd_def->arg_buf_size) {
		pr_err("command %s struct size %zu bytes, "
		       "exceeds buffer by %zu bytes\r\n",
		       cmd_def->name, cmd_def->arg_struct_size,
		       cmd_def->arg_buf_size);
		cmd_parse_cleanup(cmd_def);
		*cmd_ret = -1;
		return false;
	}
	return true;
}

static const cli_command_t *prepare_cmd_def(int argc, char **argv, int *cmd_ret)
{
	if (argc < 1) {
		*cmd_ret = 0;
		return NULL;
	}
	const cli_command_t *cmd_def = lookup_cmd_def(argv[0], cmd_ret);
	if (!cmd_def)
		return NULL;
	if (handle_help_request(cmd_def, argc, argv, cmd_ret))
		return NULL;
	if (ensure_cmd_buf(&cmd_def) < 0) {
		*cmd_ret = -1;
		return NULL;
	}
	if (!validate_cmd_buf_size(cmd_def, cmd_ret))
		return NULL;
	return cmd_def;
}

/* ============================================================
 * 新增：命令解析准备与清理（取代 dispose_mec 状态机）
 * ============================================================ */

static int execute_parsing(const cli_command_t *cmd_def, int argc, char **argv,
			   int *cmd_ret)
{
	int status = cli_auto_parse(cmd_def, argc, argv);
	if (status < 0) {
		pr_err("command parsing failed: %s\r\n", argv[0]);
		cli_print_help(cmd_def);
		cmd_parse_cleanup(cmd_def);
		*cmd_ret = -1;
		return status;
	}
	return CLI_OK;
}

int cmd_parse_prepare(char *cmd, const cli_command_t **out_cmd_def,
		      int *cmd_ret)
{
	static char *argv[CLI_MAX_ARGV];
	int argc = tokenize(cmd, argv, CLI_MAX_ARGV);
	all_printk("\r\n");
	all_printk("\033[K");
	const cli_command_t *cmd_def = prepare_cmd_def(argc, argv, cmd_ret);
	if (!cmd_def)
		return dispose_exit;
	memset(cmd_def->arg_buf, 0, cmd_def->arg_buf_size);
	int ret = execute_parsing(cmd_def, argc, argv, cmd_ret);
	if (ret < 0)
		return ret;
	*out_cmd_def = cmd_def;
	return CLI_OK;
}

void cmd_parse_cleanup(const cli_command_t *cmd_def)
{
	if (cmd_def == &cmd_runtime && cmd_runtime.arg_buf) {
		cli_mpool_free(cmd_runtime.arg_buf);
		cmd_runtime.arg_buf = NULL;
	}
}

#define CMD_CHAIN_MAX 8

int split_cmd_chain(char *buf, char **cmds, int max_cmds)
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

static int alias_handler(void *_args)
{
	struct alias_cmd *alias_cmd;
	all_printk("\r\n%-20s %-40s\r\n", "ALIAS", "ORIGINAL COMMAND");
	all_printk(
		"------------------------------------------------------------\r\n");
	FOR_EACH_ALIAS(_alias_cmd_start, _alias_cmd_end, alias_cmd)
	{
		all_printk("%-20s %-40s\r\n", alias_cmd->alias_name,
			   alias_cmd->original_name);
	}
	all_printk(
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

char *alias_replace(char *cmd, char *buf, size_t buf_size)
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

static int help_handler(void *_args)
{
	all_printk("\r\n");
	all_printk("For more information, please visit:\r\n");
	all_printk("  https://github.com/BunnyDeny/LinCLI.git\r\n");
	all_printk("\r\n");
	all_printk(
		"Tip: Press <Tab> on an empty prompt to list all commands.\r\n");
	return 0;
}

CLI_COMMAND_NO_STRUCT(help, "help", "show help information", help_handler);
