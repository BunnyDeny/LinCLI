#include "cli_candidate.h"
#include "init_d.h"
#include "cli_io.h"
#include "cmd_dispose.h"

/* ============================================================
 * 注册字符串类型命令选项候选（用于 Tab 补全）
 * ============================================================ */

CLI_CANDIDATE(log_file, "log", "file",
	      CANDIDATES("zhaolin", "inline", "inlint"));

/* ============================================================
 * 将 CLI_CANDIDATE 注册的候选数据填充到对应命令选项中
 * ============================================================
 *
 * 流程：
 *   1. 遍历所有命令
 *   2. 对每个命令遍历其选项
 *   3. 遍历每个 CLI_CANDIDATE 注册的变量
 *   4. 如果命令名和选项名匹配，则把候选数据填充到选项中
 */

void cli_candidate_init(void *arg)
{
	(void)arg;
	const cli_command_t *cmd;
	_FOR_EACH_CLI_COMMAND(cmd)
	{
		if (!cmd || !cmd->options)
			continue;
		for (size_t i = 0; i < cmd->option_count; i++) {
			cli_option_t *opt = &cmd->options[i];
			if (!opt->long_opt)
				continue;
			const cli_candidate_t *cand;
			FOR_EACH_CLI_CANDIDATE(cand)
			{
				if (!cand || !cand->cmd || !cand->long_option)
					continue;
				if (strcmp(cmd->name, cand->cmd) == 0 &&
				    strcmp(opt->long_opt, cand->long_option) == 0) {
					opt->candidate_argc = cand->argc;
					opt->candidate_argv = cand->argv;
				}
			}
		}
	}
}
_EXPORT_INIT_SYMBOL(cli_candidate_init, 12, NULL, cli_candidate_init);
