#include "cli_candidate.h"
#include "init_d.h"
#include "cli_io.h"
#include "cmd_dispose.h"

/* ============================================================
 * 注册几个字符串类型命令选项候选（用于 Tab 补全）
 * ============================================================ */

static char *log_file_argv[] = {"app.log", "app.cfg", "app.txt",
					  "debug.log", "system.log", NULL};
static char *ts_msg_argv[] = {"hello", "world", "hello world", NULL};

CLI_CANDIDATE(log_file, "log", "file", 5, log_file_argv);
CLI_CANDIDATE(ts_msg, "ts", "msg", 3, ts_msg_argv);

/* ============================================================
 * 将 CLI_CANDIDATE 注册的候选数据填充到对应命令选项中
 * ============================================================
 *
 * 三层循环：
 *   1. 遍历每个已注册的命令（.cli_commands 段）
 *   2. 遍历该命令的每个选项
 *   3. 遍历每个 CLI_CANDIDATE 注册的变量
 *   匹配条件：命令名相同 && 长选项名相同
 * ============================================================ */

static void cli_candidate_init(void *arg)
{
	(void)arg;
	const cli_command_t *cmd;
	const cli_candidate_t *cand;

	cli_printk(
		" cli_candidate_init: matching candidates to commands...\r\n");

	_FOR_EACH_CLI_COMMAND(_cli_commands_start, _cli_commands_end, cmd)
	{
		for (size_t i = 0; i < cmd->option_count; i++) {
			cli_option_t *opt = &cmd->options[i];

			FOR_EACH_CLI_CANDIDATE(cand)
			{
				if (cand->cmd && cmd->name &&
				    strcmp(cand->cmd, cmd->name) == 0 &&
				    cand->long_option && opt->long_opt &&
				    strcmp(cand->long_option, opt->long_opt) ==
					    0) {
					opt->candidate_argc = cand->argc;
					opt->candidate_argv = cand->argv;
					cli_printk(
						"  matched [%s] %s -> argc=%d [",
						cmd->name, opt->long_opt,
						opt->candidate_argc);
					for (int j = 0; j < opt->candidate_argc; j++) {
						cli_printk("%s%s",
							   opt->candidate_argv[j],
							   (j + 1 < opt->candidate_argc) ? ", " : "");
					}
					cli_printk("]\r\n");
				}
			}
		}
	}
}

/* 通过 init_d 机制在启动时自动执行匹配填充 */
_EXPORT_INIT_SYMBOL(cli_candidate_init, 12, NULL, cli_candidate_init);
