#include "cmd_dispose.h"
#include "stateM.h"
#include "cli_io.h"
#include <stdlib.h>
#include <ctype.h>

struct tStateEngine dispose_mec;
extern struct tState _dispose_start;
extern struct tState _dispose_end;

/* ============================================================
 * 字符串切分
 * ============================================================ */

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

/* ============================================================
 * 选项查找
 * ============================================================ */

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
 * 核心解析：将命令行填充到 arg_struct
 * ============================================================ */

int cli_auto_parse(const cli_command_t *cmd, int argc, char **argv,
		   void *arg_struct)
{
	if (!cmd || !argv || argc < 1 || !arg_struct)
		return -1;

	memset(arg_struct, 0, cmd->arg_struct_size);

	bool *opt_seen = calloc(cmd->option_count, sizeof(bool));
	if (!opt_seen)
		return -1;

	const cli_option_t *cur_opt = NULL;
	int cur_opt_argc = 0;
	int cur_opt_idx = 0;

	/* argv[0] 为命令名，从 1 开始 */
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];

		/* 新选项 */
		if (arg[0] == '-') {
			if (cur_opt && cur_opt->type != CLI_TYPE_BOOL &&
			    cur_opt->type != CLI_TYPE_CALLBACK &&
			    cur_opt_argc == 0) {
				pr_err("选项 -%c/--%s 缺少参数\n",
				       cur_opt->short_opt ? cur_opt->short_opt : ' ',
				       cur_opt->long_opt ? cur_opt->long_opt : "");
				free(opt_seen);
				return -1;
			}

			cur_opt = find_option(cmd, arg);
			if (!cur_opt) {
				pr_err("未知选项: %s\n", arg);
				free(opt_seen);
				return -1;
			}

			size_t idx = (size_t)(cur_opt - cmd->options);
			opt_seen[idx] = true;
			cur_opt_argc = 0;
			cur_opt_idx = 0;

			void *dst = (char *)arg_struct + cur_opt->offset;

			if (cur_opt->type == CLI_TYPE_BOOL) {
				*(bool *)dst = true;
				cur_opt = NULL;
			} else if (cur_opt->type == CLI_TYPE_CALLBACK) {
				/* CALLBACK 暂由 validator 自行处理 */
				cur_opt = NULL;
			}
			continue;
		}

		/* 值 */
		if (!cur_opt) {
			pr_err("孤立参数: %s\n", arg);
			free(opt_seen);
			return -1;
		}

		void *dst = (char *)arg_struct + cur_opt->offset;

		switch (cur_opt->type) {
		case CLI_TYPE_STRING:
			*(const char **)dst = arg;
			cur_opt = NULL;
			break;
		case CLI_TYPE_INT:
			*(int *)dst = atoi(arg);
			cur_opt = NULL;
			break;
		case CLI_TYPE_DOUBLE:
			*(double *)dst = atof(arg);
			cur_opt = NULL;
			break;
		case CLI_TYPE_INT_ARRAY: {
			if (cur_opt_idx >= (int)cur_opt->max_args) {
				pr_err("选项 -%c/--%s 参数过多\n",
				       cur_opt->short_opt ? cur_opt->short_opt : ' ',
				       cur_opt->long_opt ? cur_opt->long_opt : "");
				free(opt_seen);
				return -1;
			}
			int *arr = *(int **)dst;
			if (!arr) {
				arr = calloc(cur_opt->max_args, sizeof(int));
				*(int **)dst = arr;
			}
			arr[cur_opt_idx++] = atoi(arg);
			cur_opt_argc++;
			if (cur_opt->offset_count > 0) {
				size_t *cnt =
					(size_t *)((char *)arg_struct + cur_opt->offset_count);
				*cnt = (size_t)cur_opt_idx;
			}
			break;
		}
		default:
			pr_err("选项 -%c/--%s 类型未实现\n",
			       cur_opt->short_opt ? cur_opt->short_opt : ' ',
			       cur_opt->long_opt ? cur_opt->long_opt : "");
			free(opt_seen);
			return -1;
		}
	}

	/* 检查末尾选项是否缺参 */
	if (cur_opt && cur_opt->type != CLI_TYPE_BOOL &&
	    cur_opt->type != CLI_TYPE_CALLBACK && cur_opt_argc == 0) {
		pr_err("选项 -%c/--%s 缺少参数\n",
		       cur_opt->short_opt ? cur_opt->short_opt : ' ',
		       cur_opt->long_opt ? cur_opt->long_opt : "");
		free(opt_seen);
		return -1;
	}

	/* 检查必需选项 */
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].required && !opt_seen[i]) {
			pr_err("缺少必需选项: -%c/--%s\n",
			       cmd->options[i].short_opt ? cmd->options[i].short_opt :
							       ' ',
			       cmd->options[i].long_opt ? cmd->options[i].long_opt :
							       "");
			free(opt_seen);
			return -1;
		}
	}

	/* 检查依赖 */
	for (size_t i = 0; i < cmd->option_count; i++) {
		if (opt_seen[i] && cmd->options[i].depends) {
			bool dep_found = false;
			for (size_t j = 0; j < cmd->option_count; j++) {
				if (!opt_seen[j])
					continue;
				if (cmd->options[j].long_opt &&
				    strcmp(cmd->options[j].long_opt,
					   cmd->options[i].depends) == 0) {
					dep_found = true;
					break;
				}
				if (cmd->options[j].short_opt &&
				    cmd->options[i].depends[0] ==
					    cmd->options[j].short_opt &&
				    cmd->options[i].depends[1] == '\0') {
					dep_found = true;
					break;
				}
			}
			if (!dep_found) {
				pr_err("选项 -%c/--%s 依赖 %s 但未提供\n",
				       cmd->options[i].short_opt ?
					       cmd->options[i].short_opt :
					       ' ',
				       cmd->options[i].long_opt ?
					       cmd->options[i].long_opt :
					       "",
				       cmd->options[i].depends);
				free(opt_seen);
				return -1;
			}
		}
	}

	free(opt_seen);
	return 0;
}

int cli_parse(const char *cmd_name, int argc, char **argv, void *arg_struct)
{
	const cli_command_t *cmd = cli_command_find(cmd_name);
	if (!cmd) {
		pr_err("未知命令: %s\n", cmd_name);
		return -1;
	}
	return cli_auto_parse(cmd, argc, argv, arg_struct);
}

void cli_print_help(const cli_command_t *cmd)
{
	if (!cmd)
		return;
	pr_notice("命令: %s\n", cmd->name);
	pr_notice("描述: %s\n", cmd->doc);
	pr_notice("选项:\n");
	for (size_t i = 0; i < cmd->option_count; i++) {
		const cli_option_t *opt = &cmd->options[i];
		char req_mark[16] = "";
		char dep_mark[32] = "";
		if (opt->required)
			snprintf(req_mark, sizeof(req_mark), " [必需]");
		if (opt->depends)
			snprintf(dep_mark, sizeof(dep_mark), " [依赖:%s]", opt->depends);
		pr_notice("  -%c, --%-16s %s%s%s\n",
			  opt->short_opt ? opt->short_opt : ' ',
			  opt->long_opt ? opt->long_opt : "",
			  opt->help ? opt->help : "", req_mark, dep_mark);
	}
}

/* ============================================================
 * 状态机入口
 * ============================================================ */

int dispose_start_task(void *cmd)
{
	char *argv[64];
	int argc = tokenize((char *)cmd, argv, 64);

	if (argc < 1) {
		pr_warn("空命令\n");
		return dispose_exit;
	}

	const cli_command_t *cmd_def = cli_command_find(argv[0]);
	if (!cmd_def) {
		pr_err("未知命令: %s\n", argv[0]);
		return dispose_exit;
	}

	char arg_buf[cmd_def->arg_struct_size];
	memset(arg_buf, 0, sizeof(arg_buf));

	int status = cli_auto_parse(cmd_def, argc, argv, arg_buf);
	if (status < 0) {
		pr_err("命令解析失败: %s\n", argv[0]);
		cli_print_help(cmd_def);
		return dispose_exit;
	}

	if (cmd_def->validator) {
		cmd_def->validator(arg_buf);
	}

	return dispose_exit;
}
_EXPORT_STATE_SYMBOL(dispose_start, NULL, dispose_start_task, NULL, ".dispose");

int dispose_init(void)
{
	int status = engine_init(&dispose_mec, "dispose_start", &_dispose_start,
				 &_dispose_end);
	if (status < 0) {
		return status;
	}
	return 0;
}

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
	return 0; /* nerver */
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

static int hello_handler(void *_args)
{
	struct hello_args *args = _args;
	pr_notice("Hello command executed!\n");
	if (args->verbose)
		pr_notice("  verbose = true\n");
	if (args->name)
		pr_notice("  name    = %s\n", args->name);
	if (args->numbers && args->numbers_count > 0) {
		pr_notice("  numbers = ");
		for (size_t i = 0; i < args->numbers_count; i++)
			cli_printk(KERN_NOTICE "%d ", args->numbers[i]);
		cli_printk(KERN_NOTICE "\n");
	}
	return 0;
}

CLI_COMMAND(hello, "hello", "Print greeting message with optional name and number array", hello_handler, (struct hello_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct hello_args, verbose),
	    OPTION('n', "name", STRING, "Your name", struct hello_args, name),
	    OPTION_ARRAY('a', "array", INT_ARRAY, "Number array", struct hello_args, numbers, 8),
	    END_OPTIONS);
