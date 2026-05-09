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
 * 候选值数组辅助宏，展开为复合字面量数组指针。
 * 外层双层圆括号避免预处理器把大括号内的逗号当成宏参数分隔符。
 * ============================================================ */
#define CANDIDATES(...) \
	((char *[]){ __VA_ARGS__, NULL })

/* ============================================================
 * 注册宏：定义一个 cli_candidate_t 并将其指针放入 .cli_candidates 段
 * ============================================================
 *
 * 参数：
 *   name         - 宏实例名（用于生成内部静态变量名，需唯一）
 *   _cmd         - 该候选所归属的命令名字符串（如 "ls"）
 *   _long_option - long_option 字段初始值（如 "--file"）
 *   _argv        - 候选值字符串数组指针（通过 CANDIDATES(...) 宏定义）
 *
 * 示例：
 *   CLI_CANDIDATE(foo, "ls", "--file", CANDIDATES("a", "b", "c"));
 */

#define CLI_CANDIDATE(name, _cmd, _long_option, _argv)                   \
	static const cli_candidate_t _cli_candidate_def_##name = {         \
		.cmd = _cmd,                                                 \
		.long_option = _long_option,                                 \
		.argc = (int)((sizeof(_argv) / sizeof(char *))) - 1,             \
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
