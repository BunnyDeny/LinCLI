/*
 * LinCLI - Section boundary markers.
 *
 * 每个自定义段在 C 中定义一对 [1] = { NULL } 数组作为起始/结束标记，
 * 链接脚本按 *.start / * / *.end 的顺序收集，确保标记位于段的两端。
 * 遍历宏中的 if (((x) = *_pp) != NULL) 会自动跳过 NULL，因此不影响现有逻辑。
 */

#include "cmd_dispose.h"
#include "stateM.h"
#include "init_d.h"
#include "cli_candidate.h"
#include "cli_var.h"

/* .cli_commands */
const cli_command_t *const _cli_commands_start[1]
	__attribute__((used, section(".cli_commands.0.start"))) = { NULL };
const cli_command_t *const _cli_commands_end[1]
	__attribute__((used, section(".cli_commands.1.end"))) = { NULL };

/* .cli_cmd_line */
struct tState *const _cli_cmd_line_start[1]
	__attribute__((used, section(".cli_cmd_line.0.start"))) = { NULL };
struct tState *const _cli_cmd_line_end[1]
	__attribute__((used, section(".cli_cmd_line.1.end"))) = { NULL };

/* .scheduler */
struct tState *const _scheduler_start[1]
	__attribute__((used, section(".scheduler.0.start"))) = { NULL };
struct tState *const _scheduler_end[1]
	__attribute__((used, section(".scheduler.1.end"))) = { NULL };

/* .my_init_d */
struct init_d *const _init_d_start[1]
	__attribute__((used, section(".my_init_d.0.start"))) = { NULL };
struct init_d *const _init_d_end[1]
	__attribute__((used, section(".my_init_d.1.end"))) = { NULL };

/* .cli_candidates */
const cli_candidate_t *const _cli_candidates_start[1]
	__attribute__((used, section(".cli_candidates.0.start"))) = { NULL };
const cli_candidate_t *const _cli_candidates_end[1]
	__attribute__((used, section(".cli_candidates.1.end"))) = { NULL };

/* .cli_vars */
const cli_var_t *const _cli_vars_start[1]
	__attribute__((used, section(".cli_vars.0.start"))) = { NULL };
const cli_var_t *const _cli_vars_end[1]
	__attribute__((used, section(".cli_vars.1.end"))) = { NULL };

/* .alias_cmd */
struct alias_cmd *const _alias_cmd_start[1]
	__attribute__((used, section(".alias_cmd.0.start"))) = { NULL };
struct alias_cmd *const _alias_cmd_placeholder[1]
	__attribute__((used, section(".alias_cmd.1"))) = { NULL };
struct alias_cmd *const _alias_cmd_end[1]
	__attribute__((used, section(".alias_cmd.1.end"))) = { NULL };
