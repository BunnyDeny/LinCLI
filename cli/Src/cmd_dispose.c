#include "cmd_dispose.h"
#include "stateM.h"
#include "cli_io.h"
#include <stdlib.h>
#include <ctype.h>

struct tStateEngine dispose_mec;
extern struct tState _dispose_start;
extern struct tState _dispose_end;

char g_cli_cmd_buf[CLI_CMD_BUF_SIZE];

/**
 * @brief 在链接脚本段中按名称查找已注册的命令。
 */
static const cli_command_t *cli_command_find(const char *_name)
{
	cli_command_t *_cmd;
	_FOR_EACH_CLI_COMMAND(&_cli_commands_start, &_cli_commands_end, _cmd)
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
		arr = calloc(opt->max_args, sizeof(int));
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

static int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv,
			  void *arg_struct)
{
	if (!cmd || !argv || argc < 1 || !arg_struct)
		return -1;

	memset(arg_struct, 0, cmd->arg_struct_size);

	bool *opt_seen = calloc(cmd->option_count, sizeof(bool));
	if (!opt_seen)
		return -1;

	struct parse_state state = { 0 };

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (parse_option_switch(cmd, argv[i], arg_struct,
						opt_seen, &state) < 0) {
				free(opt_seen);
				return -1;
			}
		} else {
			if (parse_option_value(argv[i], arg_struct, &state) <
			    0) {
				free(opt_seen);
				return -1;
			}
		}
	}

	if (validate_end_state(&state) < 0 ||
	    validate_required(cmd, opt_seen) < 0 ||
	    validate_depends_and_conflicts(cmd, opt_seen) < 0) {
		free(opt_seen);
		return -1;
	}

	free(opt_seen);
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
		char req_mark[16] = "";
		char dep_mark[32] = "";
		if (opt->required)
			snprintf(req_mark, sizeof(req_mark), " [必需]");
		if (opt->depends) {
			if (opt->depends[0] == '!')
				snprintf(dep_mark, sizeof(dep_mark),
					 " [互斥:%s]", opt->depends + 1);
			else
				snprintf(dep_mark, sizeof(dep_mark),
					 " [依赖:%s]", opt->depends);
		}
		cli_printk("  -%c, --%-16s %s%s%s\n",
			   opt->short_opt ? opt->short_opt : ' ',
			   opt->long_opt ? opt->long_opt : "",
			   opt->help ? opt->help : "", req_mark, dep_mark);
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
	char *argv[64];
	int argc = tokenize((char *)cmd, argv, 64);
	cli_printk("\r\n");
	if (argc < 1) {
		return dispose_exit;
	}

	const cli_command_t *cmd_def = cli_command_find(argv[0]);
	if (!cmd_def) {
		pr_err("未知命令: %s\n", argv[0]);
		return dispose_exit;
	}

	if (has_help_flag(argc, argv)) {
		cli_print_help(cmd_def);
		return dispose_exit;
	}

	if (!cmd_def->arg_buf ||
	    cmd_def->arg_struct_size > cmd_def->arg_buf_size) {
		pr_err("命令 %s 的参数缓冲区不足 (%zu > %zu)\n", cmd_def->name,
		       cmd_def->arg_struct_size,
		       cmd_def->arg_buf ? cmd_def->arg_buf_size : 0);
		return dispose_exit;
	}

	memset(cmd_def->arg_buf, 0, cmd_def->arg_buf_size);

	int status = cli_auto_parse(cmd_def, argc, argv, cmd_def->arg_buf);
	if (status < 0) {
		pr_err("命令解析失败: %s\n", argv[0]);
		cli_print_help(cmd_def);
		return dispose_exit;
	}

	if (cmd_def->validator) {
		cmd_def->validator(cmd_def->arg_buf);
	}

	return dispose_exit;
}
_EXPORT_STATE_SYMBOL(dispose_start, NULL, dispose_start_task, NULL, ".dispose");

/**
 * @brief 初始化 dispose 状态机引擎。
 */
int dispose_init(void)
{
	int status = engine_init(&dispose_mec, "dispose_start", &_dispose_start,
				 &_dispose_end);
	if (status < 0) {
		return status;
	}
	return 0;
}

/**
 * @brief 运行 dispose 状态机，直到当前状态任务返回 dispose_exit。
 */
int dispose_task(char *cmd)
{
	int status = 0;
	while (1) {
		status = stateEngineRun(&dispose_mec, cmd);
		if (status < 0) {
			pr_err("dispose状态机异常\n");
			return -1;
		} else if (status == dispose_exit) {
			return status;
		}
	}
	return 0;
}

/* ============================================================
 * 示例命令（测试用）
 * ============================================================ */

struct hello_args {
	bool verbose;
	char *name;
	int *numbers;
	size_t numbers_count;
};

/**
 * @brief hello 命令的业务处理函数。
 */
static int hello_handler(void *_args)
{
	struct hello_args *args = _args;
	cli_printk("Hello command executed!\n");
	if (args->verbose)
		cli_printk("  verbose = true\n");
	if (args->name)
		cli_printk("  name    = %s\n", args->name);
	if (args->numbers && args->numbers_count > 0) {
		cli_printk("  numbers = ");
		for (size_t i = 0; i < args->numbers_count; i++)
			cli_printk(KERN_NOTICE "%d ", args->numbers[i]);
		cli_printk(KERN_NOTICE "\n");
	}
	return 0;
}

CLI_COMMAND(hello, "hello",
	    "Print greeting message with optional name and number array",
	    hello_handler, (struct hello_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct hello_args,
		   verbose),
	    OPTION('n', "name", STRING, "Your name", struct hello_args, name),
	    OPTION('a', "array", INT_ARRAY, "Number array", struct hello_args,
		   numbers, 8, NULL),
	    END_OPTIONS);
