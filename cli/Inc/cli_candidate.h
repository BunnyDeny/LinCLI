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

#ifndef _CLI_CANDIDATE_H_
#define _CLI_CANDIDATE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

typedef struct cli_candidate {
	char *cmd;        /* 短命令或选项名 */
	char *long_option; /* 长选项名字符串 */
	int argc;         /* 参数个数 */
	char **argv;      /* 参数列表 */
} cli_candidate_t;

/* ============================================================
 * 链接脚本段收集符号声明
 * ============================================================ */

extern const cli_candidate_t *const _cli_candidates_start[];
extern const cli_candidate_t *const _cli_candidates_end[];

/* ============================================================
 * 注册宏：定义一个 cli_candidate_t 并将其指针放入 .cli_candidates 段
 * ============================================================
 *
 * 参数：
 *   name         - 宏实例名（用于生成内部静态变量名，需唯一）
 *   _cmd         - 该候选所归属的命令名字符串（如 "ls"）
 *   _long_option - long_option 字段初始值（如 "--file"）
 *   _argc        - argv 数组有效元素个数
 *   _argv        - 候选值字符串数组指针
 *
 * 示例：
 *   CLI_CANDIDATE(foo, "ls", "--file", 0, NULL);
 */

#define CLI_CANDIDATE(name, _cmd, _long_option, _argc, _argv)            \
	static const cli_candidate_t _cli_candidate_def_##name = {         \
		.cmd = _cmd,                                                 \
		.long_option = _long_option,                                 \
		.argc = _argc,                                               \
		.argv = _argv,                                               \
	};                                                               \
	static const cli_candidate_t *const _cli_candidate_ptr_##name      \
		__attribute__((used, section(".cli_candidates.1"))) =        \
			&_cli_candidate_def_##name

/* ============================================================
 * 遍历宏：访问 .cli_candidates 段中注册的所有候选
 * ============================================================
 *
 * 使用示例：
 *   const cli_candidate_t *cand;
 *   FOR_EACH_CLI_CANDIDATE(cand) {
 *       cli_printk("cmd: %s\r\n", cand->cmd);
 *   }
 */

#define FOR_EACH_CLI_CANDIDATE(_cand)                                  \
	for (const cli_candidate_t *const *_pp = _cli_candidates_start;  \
	     _pp < (const cli_candidate_t *const *)_cli_candidates_end;  \
	     _pp++)                                                      \
		if (((_cand) = *_pp) != NULL)

#ifdef __cplusplus
}
#endif

#endif /* _CLI_CANDIDATE_H_ */
