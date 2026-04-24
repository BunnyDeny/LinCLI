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

/* .cli_commands */
const cli_command_t *const _cli_commands_start[1]
	__attribute__((used, section(".cli_commands.start"))) = { NULL };
const cli_command_t *const _cli_commands_end[1]
	__attribute__((used, section(".cli_commands.end"))) = { NULL };

/* .cli_cmd_line */
struct tState *const _cli_cmd_line_start[1]
	__attribute__((used, section(".cli_cmd_line.start"))) = { NULL };
struct tState *const _cli_cmd_line_end[1]
	__attribute__((used, section(".cli_cmd_line.end"))) = { NULL };

/* .scheduler */
struct tState *const _scheduler_start[1]
	__attribute__((used, section(".scheduler.start"))) = { NULL };
struct tState *const _scheduler_end[1]
	__attribute__((used, section(".scheduler.end"))) = { NULL };

/* .my_init_d */
struct init_d *const _init_d_start[1]
	__attribute__((used, section(".my_init_d.start"))) = { NULL };
struct init_d *const _init_d_end[1]
	__attribute__((used, section(".my_init_d.end"))) = { NULL };

/* .dispose */
struct tState *const _dispose_start[1]
	__attribute__((used, section(".dispose.start"))) = { NULL };
struct tState *const _dispose_end[1]
	__attribute__((used, section(".dispose.end"))) = { NULL };

/* .alias_cmd */
struct alias_cmd *const _alias_cmd_start[1]
	__attribute__((used, section(".alias_cmd.start"))) = { NULL };
struct alias_cmd *const _alias_cmd_end[1]
	__attribute__((used, section(".alias_cmd.end"))) = { NULL };
