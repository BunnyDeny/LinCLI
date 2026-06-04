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

#include "cmd_dispose.h"
#include "cli_errno.h"
#include "cli_user.h"
#include "stateM.h"
#include "cli_io.h"
#include "cli_cmd_line.h"
#include "cli_mpool.h"
#include "cli_parse.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define CLI_HELP_REQ_MARK_SIZE 16
#define CLI_HELP_DEP_MARK_SIZE 128

/**
 * @brief 在链接脚本段中按名称查找已注册的命令。
 */
static const cli_command_t *cli_command_find(const char *_name)
{
	const cli_command_t *_cmd;
	_FOR_EACH_CLI_COMMAND(_cmd)
	{
		if (_cmd->name && strcmp(_cmd->name, _name) == 0)
			return _cmd;
	}
	return NULL;
}

/**
 * @brief 将一行输入字符串切分为 argc/argv 形式。
 *
 * 支持单引号 ' 和双引号 " 包裹的字符串作为不可再分的最小单元，
 * 引号内的内容不会被空白字符或命令链操作符分割。
 */
static inline bool is_space(char c)
{
	return c == ' ' || c == '\t';
}

static char *skip_spaces(char *p)
{
	while (is_space(*p))
		p++;
	return p;
}

static char *extract_quoted(char **p_out)
{
	char *p = *p_out;
	char quote = *p++;
	char *start = p;

	while (*p && *p != quote)
		p++;
	if (*p == quote)
		*p++ = '\0';
	*p_out = p;
	return start;
}

static char *extract_unquoted(char **p_out)
{
	char *p = *p_out;
	char *start = p;

	while (*p && !is_space(*p)) {
		if (*p == '\'' || *p == '"') {
			char quote = *p++;
			while (*p && *p != quote)
				p++;
			if (*p == quote)
				p++;
		} else
			p++;
	}
	*p_out = p;
	return start;
}

static int tokenize(char *line, char **argv, int max_argv)
{
	int argc = 0;
	char *p = line;

	while (*p && argc < max_argv) {
		p = skip_spaces(p);
		if (!*p)
			break;
		if (*p == '\'' || *p == '"')
			argv[argc++] = extract_quoted(&p);
		else
			argv[argc++] = extract_unquoted(&p);
		if (*p && is_space(*p))
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
		pr_err("opt -%c/--%s need arg\r\n",
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
		pr_err("unk opt: %s\r\n", arg);
		return CLI_ERR_UNKNOWN_OPT;
	}
	return 0;
}

static int mark_option_seen(cli_option_t *opt, bool *opt_seen,
			    struct parse_state *state)
{
	size_t idx = (size_t)(opt - state->cmd->options);
	if (opt_seen[idx]) {
		pr_err("dup -%c/--%s\r\n",
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



static int *ensure_int_array(cli_option_t *opt, void *arg_struct,
			     struct parse_state *state)
{
	void *dst = (char *)arg_struct + opt->offset;
	int *arr = *(int **)dst;
	if (arr)
		return arr;
	size_t need = opt->max_args * sizeof(int);
	if (need > state->scratch_remain) {
		int shortfall = (int)need - (int)state->scratch_remain;
		pr_err("opt -%c/--%s buf %d/%u\r\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "", shortfall,
		       (unsigned int)need);
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
		pr_err("opt -%c/--%s max args\r\n",
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
	case CLI_TYPE_FLOAT:
		return cli_parse_float(arg, (float *)dst);
	case CLI_TYPE_INT_ARRAY:
		return parse_int_array(state->cur_opt, arg, arg_struct, state);
	default:
		pr_err("opt -%c/--%s bad type\r\n",
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
		pr_err("orphan %s\r\n", arg);
		return CLI_ERR_ORPHAN_ARG;
	}
	int ret = parse_value_by_type(arg, arg_struct, state);
	if (ret < 0)
		return ret;
	if (state->cur_opt->type != CLI_TYPE_INT_ARRAY)
		state->cur_opt = NULL;
	return CLI_OK;
}

static int validate_required(const cli_command_t *cmd, const bool *opt_seen)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].required && !opt_seen[i]) {
			pr_err("need -%c/--%s\r\n",
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
		pr_err("opt -%c/--%s need %s\r\n",
		       opt->short_opt ? opt->short_opt : ' ',
		       opt->long_opt ? opt->long_opt : "", name_buf);
	} else {
		pr_err("opt -%c/--%s conflict %s\r\n",
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

static int validate_constraints(const cli_command_t *cmd, const bool *opt_seen)
{
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (!opt_seen[i])
			continue;
		if (cmd->options[i].depends) {
			int ret = check_name_list(cmd, opt_seen, i,
						  cmd->options[i].depends, true,
						  CLI_ERR_DEP_MISSING);
			if (ret < 0)
				return ret;
		}
		if (cmd->options[i].conflicts) {
			int ret = check_name_list(cmd, opt_seen, i,
						  cmd->options[i].conflicts, false,
						  CLI_ERR_CONFLICT);
			if (ret < 0)
				return ret;
		}
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
		int shortfall = (int)need - (int)state->scratch_remain;
		pr_err("str too long %d/%u\r\n",
		       shortfall, (unsigned int)need);
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
	ret = check_prev_opt_missing_arg(state);
	if (ret < 0)
		return ret;
	ret = validate_required(cmd, opt_seen);
	if (ret < 0)
		return ret;
	ret = validate_constraints(cmd, opt_seen);
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
		pr_err("cmd %s buf %d\r\n",
		       cmd->name, (int)need - (int)avail);
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
#if CLI_ENABLE_HELP
static void build_opt_marks(cli_option_t *opt, char *req_mark,
			    char *dep_mark, size_t dep_mark_size)
{
	req_mark[0] = '\0';
	dep_mark[0] = '\0';
	if (opt->required)
		snprintf(req_mark, CLI_HELP_REQ_MARK_SIZE, " [R]");
	if (opt->depends && opt->depends[0]) {
		snprintf(dep_mark, dep_mark_size, " [D:%s]",
			 opt->depends);
	}
	if (opt->conflicts && opt->conflicts[0]) {
		size_t len = strlen(dep_mark);
		if (len < dep_mark_size - 1) {
			snprintf(dep_mark + len, dep_mark_size - len,
				 " [conf:%s]", opt->conflicts);
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

static void print_cmd_brief(const cli_command_t *cmd)
{
	all_printk("%s", cmd->name);
	if (cmd->brief)
		all_printk(" - %s", cmd->brief);
	all_printk("\r\n");
	if (cmd->usage && cmd->usage_count > 0) {
		all_printk("use: %s\r\n", cmd->usage[0]);
		for (int i = 1; i < cmd->usage_count; i++)
			all_printk("  %s\r\n", cmd->usage[i]);
	}
}

static void print_opt_line(cli_option_t *opt, const char *req_mark,
			   const char *dep_mark)
{
	all_printk("  -%c, --%-12s %s%s%s\r\n",
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

	print_cmd_brief(cmd);
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
#endif /* CLI_ENABLE_HELP */

/**
 * @brief dispose 状态机的起始任务，完成完整的命令分派闭环。
 */
static const cli_command_t *lookup_cmd_def(char *cmd_name, int *cmd_ret)
{
	const cli_command_t *cmd_def = cli_command_find(cmd_name);
	if (!cmd_def) {
		pr_err("unk cmd: %s\r\n", cmd_name);
		*cmd_ret = -1;
	}
	return cmd_def;
}

static int ensure_cmd_buf(const cli_command_t **cmd_def_p,
			  cmd_parse_ctx_t *ctx)
{
	const cli_command_t *cmd_def = *cmd_def_p;
	if (cmd_def->arg_buf)
		return 0;
	memcpy(&ctx->cmd_runtime, cmd_def, sizeof(cli_command_t));
	ctx->cmd_runtime.arg_buf = cli_mpool_alloc();
	if (!ctx->cmd_runtime.arg_buf) {
		pr_err("%s OOM\r\n", cmd_def->name);
		return -1;
	}
	*cmd_def_p = &ctx->cmd_runtime;
	return 0;
}

static bool validate_cmd_buf_size(const cli_command_t *cmd_def, int *cmd_ret,
				    cmd_parse_ctx_t *ctx)
{
	if (cmd_def->arg_struct_size > cmd_def->arg_buf_size) {
		pr_err("cmd %s sz %u>%u\r\n",
		       cmd_def->name, (unsigned int)cmd_def->arg_struct_size,
		       (unsigned int)cmd_def->arg_buf_size);
		cmd_parse_cleanup(cmd_def, ctx);
		*cmd_ret = -1;
		return false;
	}
	return true;
}

static const cli_command_t *prepare_cmd_def(int argc, char **argv,
					    int *cmd_ret,
					    cmd_parse_ctx_t *ctx)
{
	if (argc < 1) {
		*cmd_ret = 0;
		return NULL;
	}
	const cli_command_t *cmd_def = lookup_cmd_def(argv[0], cmd_ret);
	if (!cmd_def)
		return NULL;
	if (!cli_user_cmd_permitted(cmd_def)) {
		pr_err("denied: %s/%s\r\n",
		       argv[0], current_user->username);
		*cmd_ret = -1;
		return NULL;
	}
#if CLI_ENABLE_HELP
	if (handle_help_request(cmd_def, argc, argv, cmd_ret))
		return NULL;
#endif
	if (ensure_cmd_buf(&cmd_def, ctx) < 0) {
		*cmd_ret = -1;
		return NULL;
	}
	if (!validate_cmd_buf_size(cmd_def, cmd_ret, ctx))
		return NULL;
	return cmd_def;
}

/* ============================================================
 * 新增：命令解析准备与清理（取代 dispose_mec 状态机）
 * ============================================================ */

#if CLI_ENABLE_HELP
static void cli_print_usage(const cli_command_t *cmd)
{
	if (!cmd || !cmd->usage || cmd->usage_count <= 0)
		return;
	all_printk("use: %s\r\n", cmd->usage[0]);
	for (int i = 1; i < cmd->usage_count; i++)
		all_printk("  %s\r\n", cmd->usage[i]);
}
#endif

static int execute_parsing(const cli_command_t *cmd_def, int argc, char **argv,
			   int *cmd_ret, cmd_parse_ctx_t *ctx)
{
	int status = cli_auto_parse(cmd_def, argc, argv);
	if (status < 0) {
		pr_err("parse err: %s\r\n", argv[0]);
#if CLI_ENABLE_HELP
		cli_print_usage(cmd_def);
		pr_err("use %s -h\r\n", cmd_def->name);
#endif
		cmd_parse_cleanup(cmd_def, ctx);
		*cmd_ret = -1;
		return status;
	}
	return CLI_OK;
}

int cmd_parse_prepare(char *cmd, cmd_parse_ctx_t *ctx,
		      const cli_command_t **out_cmd_def, int *cmd_ret)
{
	int argc = tokenize(cmd, ctx->argv, CLI_MAX_ARGV);
	all_printk("\r\n");
	all_printk("\033[K");
	const cli_command_t *cmd_def = prepare_cmd_def(argc, ctx->argv,
						       cmd_ret, ctx);
	if (!cmd_def)
		return dispose_exit;
	if (is_raw_command(cmd_def)) {
		raw_cmd_args_t *raw_args =
			(raw_cmd_args_t *)cmd_def->arg_buf;
		raw_args->argc = argc;
		raw_args->argv = ctx->argv;
		*out_cmd_def = cmd_def;
		return CLI_OK;
	}
	memset(cmd_def->arg_buf, 0, cmd_def->arg_buf_size);
	int ret = execute_parsing(cmd_def, argc, ctx->argv, cmd_ret, ctx);
	if (ret < 0)
		return ret;
	*out_cmd_def = cmd_def;
	return CLI_OK;
}

void cmd_parse_cleanup(const cli_command_t *cmd_def, cmd_parse_ctx_t *ctx)
{
	if (cmd_def == &ctx->cmd_runtime && ctx->cmd_runtime.arg_buf) {
		cli_mpool_free(ctx->cmd_runtime.arg_buf);
		ctx->cmd_runtime.arg_buf = NULL;
	}
}

#if CLI_ENABLE_CMD_CHAIN
#define CMD_CHAIN_MAX 8

static char *trim_tail_spaces(char *start, char *end)
{
	while (end > start && *end == ' ')
		*end-- = '\0';
	return end;
}

static char *skip_quoted_block(char *p)
{
	char quote = *p++;

	while (*p && *p != quote)
		p++;
	if (*p == quote)
		p++;
	return p;
}

static char *find_chain_split(char *p, char **split_pos)
{
	while (*p) {
		if (*p == '\'' || *p == '"') {
			p = skip_quoted_block(p);
		} else if (p[0] == '&' && p[1] == '&') {
			*split_pos = p;
			return p;
		} else {
			p++;
		}
	}
	*split_pos = NULL;
	return p;
}

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
		char *split_pos = NULL;
		p = find_chain_split(p, &split_pos);
		if (split_pos) {
			*split_pos = '\0';
			trim_tail_spaces(cmds[cnt - 1], split_pos - 1);
			p = split_pos + 2;
		} else {
			char *end = p + strlen(p) - 1;
			trim_tail_spaces(cmds[cnt - 1], end);
			break;
		}
	}
	return cnt;
}
#endif /* CLI_ENABLE_CMD_CHAIN */
