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
 * @brief 将一行输入字符串按空白字符切分为 argc/argv 形式。
 *
 * @param[in,out] line    原始命令行字符串，函数会在分隔处插入 '\0' 进行破坏式切分。
 * @param[out]    argv    切分后得到的字符串指针数组，argv[0] 为命令名本身。
 * @param[in]     max_argv argv 数组的最大容量，防止越界。
 *
 * @return 切分后的参数个数 argc（包含 argv[0] 命令名）。
 *
 * @note 目前仅支持空格与制表符作为分隔符，不支持引号包裹的含空格参数。
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
 *
 * @param[in] cmd  命令定义结构体，包含所有已注册选项的数组。
 * @param[in] arg  用户输入的当前参数，形如 "-v" 或 "--verbose"。
 *
 * @return 匹配到的选项指针；若未找到或 arg 格式不合法，返回 NULL。
 *
 * @note 短选项仅支持单字符（如 "-v"），"-abc" 这种组合形式会被拒绝。
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

/**
 * @brief 自动解析给定命令的选项，并将解析结果填充到开发者提供的参数结构体中。
 *
 * @param[in]  cmd         命令定义，包含选项数组及校验规则。
 * @param[in]  argc        参数个数（包含 argv[0] 命令名）。
 * @param[in]  argv        参数字符串数组。
 * @param[out] arg_struct  开发者定义的参数结构体指针，函数会按各选项的 offset
 *                         将转换后的值写入对应内存位置。
 *
 * @return  0  解析成功；
 *         -1  参数非法、选项未知、缺少参数、参数过多、必需项缺失或依赖未满足。
 *
 * @details 解析流程如下：
 *   1. 清零 arg_struct；
 *   2. 遍历 argv[1..argc-1]，识别选项标记（以 '-' 开头）；
 *   3. 根据 cli_option_t::type 进行类型转换（BOOL、STRING、INT、DOUBLE、INT_ARRAY）；
 *   4. 对于 INT_ARRAY，会按需 calloc 数组内存，并通过 offset_count 更新计数器；
 *   5. 遍历结束后进行完整性校验：末尾缺参、required 缺失、depends 未满足。
 */
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

	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (arg[0] == '-') {
			if (cur_opt && cur_opt->type != CLI_TYPE_BOOL &&
			    cur_opt->type != CLI_TYPE_CALLBACK &&
			    cur_opt_argc == 0) {
				pr_err("选项 -%c/--%s 缺少参数\n",
				       cur_opt->short_opt ? cur_opt->short_opt :
							    ' ',
				       cur_opt->long_opt ? cur_opt->long_opt :
							   "");
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
				cur_opt = NULL;
			}
			continue;
		}

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
				       cur_opt->short_opt ? cur_opt->short_opt :
							    ' ',
				       cur_opt->long_opt ? cur_opt->long_opt :
							   "");
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
				size_t *cnt = (size_t *)((char *)arg_struct +
							 cur_opt->offset_count);
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

	if (cur_opt && cur_opt->type != CLI_TYPE_BOOL &&
	    cur_opt->type != CLI_TYPE_CALLBACK && cur_opt_argc == 0) {
		pr_err("选项 -%c/--%s 缺少参数\n",
		       cur_opt->short_opt ? cur_opt->short_opt : ' ',
		       cur_opt->long_opt ? cur_opt->long_opt : "");
		free(opt_seen);
		return -1;
	}

	for (size_t i = 0; i < cmd->option_count; i++) {
		if (cmd->options[i].required && !opt_seen[i]) {
			pr_err("缺少必需选项: -%c/--%s\n",
			       cmd->options[i].short_opt ?
				       cmd->options[i].short_opt :
				       ' ',
			       cmd->options[i].long_opt ?
				       cmd->options[i].long_opt :
				       "");
			free(opt_seen);
			return -1;
		}
	}

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

/**
 * @brief 打印指定命令的帮助信息。
 *
 * @param[in] cmd  命令定义指针。若为 NULL，函数直接返回。
 *
 * @note 输出内容包括命令名、doc 描述、所有选项的短/长名称、帮助文本、
 *       是否必需以及依赖关系。格式通过 pr_notice 输出。
 */
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
			snprintf(dep_mark, sizeof(dep_mark), " [依赖:%s]",
				 opt->depends);
		pr_notice("  -%c, --%-16s %s%s%s\n",
			  opt->short_opt ? opt->short_opt : ' ',
			  opt->long_opt ? opt->long_opt : "",
			  opt->help ? opt->help : "", req_mark, dep_mark);
	}
}

/**
 * @brief 检查命令行参数中是否包含帮助请求标志。
 *
 * @param[in] argc  参数个数（包含 argv[0]）。
 * @param[in] argv  参数数组。
 *
 * @return 若 argv 中存在 "-h" 或 "--help" 则返回 true，否则返回 false。
 *
 * @note 扫描范围从 argv[1] 开始，跳过命令名本身。
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
 *
 * @param[in] cmd  用户输入的原始命令行字符串（由调度器传入）。
 *
 * @return 固定返回 dispose_exit，表示本次命令处理完毕，状态机可切回等待输入。
 *
 * @details 执行流程：
 *   1. tokenize 切分命令行；
 *   2. 通过 cli_command_find 查找命令定义；
 *   3. 若包含 -h/--help，直接打印帮助并返回；
 *   4. 检查命令的 arg_buf 是否足以容纳参数结构体；
 *   5. 使用 cmd_def->arg_buf 调用 cli_auto_parse 解析并校验选项；
 *   6. 解析失败时打印帮助信息；
 *   7. 解析成功后调用 cmd_def->validator（即开发者注册的 handler）。
 *
 * @note 默认使用 CLI_COMMAND 注册的命令共享同一块全局缓冲区 g_cli_cmd_buf，
 *       在串行执行前提下零额外内存开销。需要独立大缓冲区时，请用
 *       CLI_COMMAND_WITH_BUF 宏自行指定内存。
 */
int dispose_start_task(void *cmd)
{
	char *argv[64];
	int argc = tokenize((char *)cmd, argv, 64);

	if (argc < 1) {
		cli_printk("\r\n");
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
		pr_err("命令 %s 的参数缓冲区不足 (%zu > %zu)\n",
		       cmd_def->name, cmd_def->arg_struct_size,
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
 *
 * @return  0  初始化成功；
 *         <0  engine_init 返回的错误码。
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
 *
 * @param[in] cmd  当前待处理的命令行字符串，会作为 private 参数传递给状态任务。
 *
 * @return  0  正常结束；
 *         -1  状态机运行过程中出现异常（返回负值）。
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
 *
 * @param[in] _args  指向已填充好的 hello_args 结构体的指针。
 *
 * @return 固定返回 0。
 *
 * @note 该函数通过 CLI_COMMAND 宏注册为 validator，由 dispose_start_task
 *       在解析成功后调用。
 */
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

CLI_COMMAND(hello, "hello",
	    "Print greeting message with optional name and number array",
	    hello_handler, (struct hello_args *)0,
	    OPTION('v', "verbose", BOOL, "Enable verbose", struct hello_args,
		   verbose),
	    OPTION('n', "name", STRING, "Your name", struct hello_args, name),
	    OPTION('a', "array", INT_ARRAY, "Number array",
		   struct hello_args, numbers, 8),
	    END_OPTIONS);
