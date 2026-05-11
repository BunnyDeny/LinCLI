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

#ifndef _CMD_DISPOSE_H_
#define _CMD_DISPOSE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
//#include <getopt.h>
#include <assert.h>

#define dispose_exit 1

/* ============================================================
 * 类型系统定义
 * ============================================================ */

typedef enum {
	CLI_TYPE_BOOL, // 开关类型，无参数
	CLI_TYPE_STRING, // 字符串类型
	CLI_TYPE_INT, // 单个整数
	CLI_TYPE_INT_ARRAY, // 整数数组
	CLI_TYPE_FLOAT, // 浮点数
	CLI_TYPE_CALLBACK, // 自定义回调处理
	CLI_TYPE_CUSTOM, // 自定义变量类型（cli_var 专用）
} cli_type_t;

typedef struct cli_option {
	char short_opt; // 短选项，如 'v'
	const char *long_opt; // 长选项，如 "verbose"
	cli_type_t type; // 类型
	const char *help; // 帮助文本
	size_t offset; // 在结构体中的偏移量
	size_t offset_count; // 数组长度的偏移量（仅用于数组类型）
	size_t max_args; // 最大参数个数（数组类型用）
	bool required; // 是否必需
	const char *depends; // 依赖列表（空格分隔的长选项名）
	const char *conflicts; // 互斥列表（空格分隔的长选项名）
	int candidate_argc; // 候选值个数（字符串类型选项的 Tab 补全）
	char **candidate_argv; // 候选值列表
} cli_option_t;

typedef struct cli_command {
	const char *name; // 命令名
	const char *brief; // 命令简介
	char **usage; // 用法字符串数组（NULL 表示无用法说明）
	int usage_count; // 用法数量
	void *arg_struct; // 参数结构体指针（运行时填充）
	size_t arg_struct_size; // 结构体大小
	cli_option_t *options; // 选项数组
	size_t option_count; // 选项数量
	void (*cmd_entry)(void *); // 命令入口，只执行一次
	int (*cmd_task)(void *); // 命令主体，每次调度器轮询执行一次
	void (*cmd_exit)(void *); // 命令出口，只执行一次
	void *arg_buf; // 命令参数解析缓冲区指针
	size_t arg_buf_size; // 缓冲区大小
} cli_command_t;

/* ============================================================
 * 链接脚本段收集符号声明
 * ============================================================ */

extern const cli_command_t *const _cli_commands_start[];
extern const cli_command_t *const _cli_commands_end[];

#define _FOR_EACH_CLI_COMMAND(_cmd)                               \
	for (const cli_command_t *const *_pp = _cli_commands_start; \
	     _pp < (const cli_command_t *const *)_cli_commands_end; _pp++) \
		if (((_cmd) = *_pp) != NULL)

/* ============================================================
 * 宏工具：计算偏移量
 * ============================================================ */

#define CLI_OFFSETOF(type, field) offsetof(type, field)

#define CLI_MAX_ARGV 64

typedef struct {
	char *argv[CLI_MAX_ARGV];
	cli_command_t cmd_runtime;
} cmd_parse_ctx_t;

/* ============================================================
 * Raw argument command support
 * ============================================================ */

typedef struct {
	int argc;
	char **argv;
} raw_cmd_args_t;

static inline bool is_raw_command(const cli_command_t *cmd)
{
	return cmd && cmd->option_count == 1 && cmd->options &&
	       cmd->options[0].long_opt &&
	       strcmp(cmd->options[0].long_opt, "raw") == 0;
}

#if CLI_ENABLE_RAW_COMMAND
#define CLI_RAW_COMMAND(name, cmd_str, brief_str, _usage_arr, handler, \
			   ...)                                                \
	static char *_cli_raw_cands_##name[] = { __VA_ARGS__, NULL };    \
	cli_option_t _cli_options_##name[] = {                         \
		{                                                          \
			.short_opt = 0,                                        \
			.long_opt = "raw",                                     \
			.type = CLI_TYPE_STRING,                               \
			.help = "raw command (no options)",                    \
			.offset = 0,                                           \
			.offset_count = 0,                                     \
			.max_args = 0,                                         \
			.required = false,                                     \
			.depends = NULL,                                       \
			.conflicts = NULL,                                     \
			.candidate_argc =                                      \
				(int)((sizeof(_cli_raw_cands_##name) /               \
				       sizeof(char *))) -                            \
				1,                                                 \
			.candidate_argv = _cli_raw_cands_##name,                 \
		},                                                         \
	};                                                             \
	static int _cli_raw_wrap_task_##name(void *_a)                   \
	{                                                              \
		raw_cmd_args_t *a = (raw_cmd_args_t *)_a;                  \
		return handler(a->argv, a->argc);                          \
	}                                                              \
	_EXPORT_CLI_COMMAND_SYMBOL(                                    \
		name, cmd_str, brief_str, _usage_arr, 0,                   \
		_cli_options_##name, 1,                                    \
		NULL, (int (*)(void *))_cli_raw_wrap_task_##name, NULL,    \
		NULL, CLI_CMD_BUF_SIZE, ".cli_commands")

#define CLI_RAW_COMMAND_ASYNC(name, cmd_str, brief_str, _usage_arr,  \
				  _entry, _task, _exit, ...)                       \
	static char *_cli_raw_cands_##name[] = { __VA_ARGS__, NULL };    \
	cli_option_t _cli_options_##name[] = {                         \
		{                                                          \
			.short_opt = 0,                                        \
			.long_opt = "raw",                                     \
			.type = CLI_TYPE_STRING,                               \
			.help = "raw command (no options)",                    \
			.offset = 0,                                           \
			.offset_count = 0,                                     \
			.max_args = 0,                                         \
			.required = false,                                     \
			.depends = NULL,                                       \
			.conflicts = NULL,                                     \
			.candidate_argc =                                      \
				(int)((sizeof(_cli_raw_cands_##name) /               \
				       sizeof(char *))) -                            \
				1,                                                 \
			.candidate_argv = _cli_raw_cands_##name,                 \
		},                                                         \
	};                                                             \
	static void _cli_raw_wrap_entry_##name(void *_a)                 \
	{                                                              \
		raw_cmd_args_t *a = (raw_cmd_args_t *)_a;                  \
		_entry(a->argv, a->argc);                                  \
	}                                                              \
	static int _cli_raw_wrap_task_##name(void *_a)                   \
	{                                                              \
		raw_cmd_args_t *a = (raw_cmd_args_t *)_a;                  \
		return _task(a->argv, a->argc);                            \
	}                                                              \
	static void _cli_raw_wrap_exit_##name(void *_a)                  \
	{                                                              \
		raw_cmd_args_t *a = (raw_cmd_args_t *)_a;                  \
		_exit(a->argv, a->argc);                                   \
	}                                                              \
	_EXPORT_CLI_COMMAND_SYMBOL(                                    \
		name, cmd_str, brief_str, _usage_arr, 0,                   \
		_cli_options_##name, 1,                                    \
		(void (*)(void *))_cli_raw_wrap_entry_##name,              \
		(int (*)(void *))_cli_raw_wrap_task_##name,                \
		(void (*)(void *))_cli_raw_wrap_exit_##name,               \
		NULL, CLI_CMD_BUF_SIZE, ".cli_commands")

#else
#define CLI_RAW_COMMAND(...) /* disabled */
#define CLI_RAW_COMMAND_ASYNC(...) /* disabled */
#endif

/* ============================================================
 * OPTION 宏定义（统一 10 参数）
 * ============================================================
 *
 * 注册一个命令选项。所有类型（BOOL / STRING / INT / DOUBLE / CALLBACK / INT_ARRAY）
 * 所有选项统一使用 10 个参数。
 *
 * ------------------------------------------------------------------
 * 参数详解
 * ------------------------------------------------------------------
 *
 *   1. _sopt  (char)
 *      短选项字符。终端输入 -o 时匹配。若不需要短选项，填 0。
 *
 *   2. _lopt  (const char *)
 *      长选项名字符串。终端输入 --on 时匹配。框架同时支持短选项
 *      和长选项，用户可任选其一输入。
 *
 *   3. _type  (标识符)
 *      选项类型。可选：
 *        BOOL      - 开关型，无参数，出现即置 true
 *        STRING    - 字符串参数
 *        INT       - 单个整数参数
 *        DOUBLE    - 浮点数参数
 *        CALLBACK  - 自定义回调，原始字符串透传给 handler
 *        INT_ARRAY - 整数数组，可接收多个整数参数
 *
 *   4. _help  (const char *)
 *      帮助文本。执行 <命令> --help 时显示在该选项后方。
 *
 *   5. _stype (类型名)
 *      参数结构体类型。必须与 CLI_COMMAND 第 5 个参数推导出的
 *      类型一致，例如 struct led_args。
 *
 *   6. _field (标识符)
 *      该选项对应结构体中的字段名。框架解析成功后，结果会自动
 *      写入 args->_field。
 *      对于 INT_ARRAY，该字段必须是 int * 类型；框架会自动寻找
 *      同名的 _count 字段（如 nums → nums_count）存放实际解析到
 *      的元素个数。
 *
 *   7. _max   (size_t)
 *      最大参数个数。
 *      - 仅 INT_ARRAY 有意义：表示该数组选项最多接收多少个整数，
 *        同时框架会在 arg_buf 尾部静态预留 _max * sizeof(int) 字节
 *        的连续空间。
 *      - 对于非数组类型，该字段不会被使用，固定填 0。
 *
 *   8. _dep   (const char *)
 *      依赖列表。空格分隔的多个长选项名字符串。表示：只有当列表
 *      中列出的所有选项都出现时，本选项才是合法的。不需要依赖时
 *      填 NULL。
 *      示例："verbose debug" 表示本选项依赖 --verbose 和 --debug
 *      同时出现。
 *
 *   9. _con   (const char *)
 *      互斥列表。空格分隔的多个长选项名字符串。表示：列表中列出
 *      的任一选项出现时，本选项不能出现。不需要互斥时填 NULL。
 *
 *      【设计原则】互斥是单向声明的。如果 -a 与 -b 互斥，只需在
 *      -a 的 _con 中写 "b"，或在 -b 的 _con 中写 "a"，即可覆盖
 *      整个互斥关系。框架会在该选项被输入时，检查其互斥列表中的
 *      目标是否出现；若出现则报错。
 *      当然，如果你愿意在双方的 _con 中都写上对方，也是完全合法
 *      的，效果等价。
 *      示例："off reset" 表示本选项与 --off 和 --reset 互斥。
 *
 *  10. _req   (bool)
 *      是否为必需选项。true 表示用户必须提供该选项，否则框架报
 *      "缺少必需选项"错误。
 *
 * ------------------------------------------------------------------
 * 使用示例
 * ------------------------------------------------------------------
 *
 *   OPTION('o', "on",  BOOL, "Turn LED on",
 *          struct led_args, on, 0, "brightness", "off", false)
 *   OPTION('f', "off", BOOL, "Turn LED off",
 *          struct led_args, off, 0, NULL, "on", false)
 *   OPTION('b', "brightness", INT, "Brightness 0-100",
 *          struct led_args, brightness, 0, "on", NULL, false)
 *   OPTION('t', "tags", INT_ARRAY, "Tag list",
 *          struct log_args, tags, 8, "verbose", NULL, false)
 *   OPTION('f', "file", STRING, "Log file path",
 *          struct log_args, file, 0, NULL, NULL, true)
 */

#define OPTION(_sopt, _lopt, _type, _help, _stype, _field, _max, _dep, _con, \
	       _req)                                                         \
	{                                                                    \
		.short_opt = _sopt,                                          \
		.long_opt = _lopt,                                           \
		.type = CLI_TYPE_##_type,                                    \
		.help = _help,                                               \
		.offset = CLI_OFFSETOF(_stype, _field),                      \
		.offset_count = _OPTION_COUNT_##_type(_stype, _field),       \
		.max_args = _max,                                            \
		.required = _req,                                            \
		.depends = _dep,                                             \
		.conflicts = _con,                                           \
	}

/* 各类型的 offset_count 计算 */
#define _OPTION_COUNT_BOOL(_stype, _field) 0
#define _OPTION_COUNT_STRING(_stype, _field) 0
#define _OPTION_COUNT_INT(_stype, _field) 0
#define _OPTION_COUNT_DOUBLE(_stype, _field) 0
#define _OPTION_COUNT_FLOAT(_stype, _field) 0
#define _OPTION_COUNT_CALLBACK(_stype, _field) 0
#define _OPTION_COUNT_INT_ARRAY(_stype, _field) \
	CLI_OFFSETOF(_stype, _field##_count)

/* ============================================================
 * 非阻塞命令执行控制宏
 * ============================================================
 *
 * cmd_task 返回值语义：
 *   ret < 0  : 执行失败，退出命令，返回值由调度器记录
 *   ret == 0 : 执行成功，退出命令
 *   ret == CLI_CONTINUE : 本次执行完毕，但命令不结束，下次 scheduler 轮询继续
 *   ret > 1  : 扩展语义，视为执行成功并退出
 */

#define CLI_CONTINUE 1 /* 继续执行，下次 scheduler 轮询再来 */

/* ============================================================
 * CLI_COMMAND宏：注册一个命令（通过链接脚本段收集）
 * ============================================================
 *
 * 参数名前加下划线，避免与 cli_command_t 成员名冲突导致
 * 指定初始化器被意外替换。
 *
 * 警告：arg_struct_ptr 必须传入类型明确的结构体指针表达式，
 * 例如 (struct xxx *)0 或 &((struct xxx){0})，否则 typeof 无法
 * 正确推导类型。严禁传入 NULL，否则 sizeof 会退化为 1，
 * 导致运行时缓冲区分配错误。
 */

#define _EXPORT_CLI_COMMAND_SYMBOL(_obj, _cmd_str, _brief_str, _usage,      \
				   _size, _opts, _opts_cnt, _entry,    \
				   _task, _exit, _buf, _buf_size, _section)  \
	static const cli_command_t _cli_cmd_def_##_obj = {                     \
		.name = _cmd_str,                                              \
		.brief = _brief_str,                                           \
		.usage = _usage,                                               \
		.usage_count = (int)((sizeof(_usage) / sizeof(char *)) - 1),   \
		.arg_struct = NULL,                                            \
		.arg_struct_size = _size,                                      \
		.options = _opts,                                              \
		.option_count = _opts_cnt,                                     \
		.cmd_entry = _entry,                                           \
		.cmd_task = _task,                                             \
		.cmd_exit = _exit,                                             \
		.arg_buf = _buf,                                               \
		.arg_buf_size = _buf_size,                                     \
	};                                                                     \
	static const cli_command_t *const _cli_cmd_ptr_##_obj                  \
		__attribute__((used, section(_section ".1"))) =                \
			&_cli_cmd_def_##_obj

#include "cli_config.h"

/* 使用 CLI_COMMAND 注册的命令在运行时通过内存池动态分配参数缓冲区。
 * 若用户结构体超过内存池单块大小，请使用 CLI_COMMAND_WITH_BUF 宏自行指定缓冲区。
 */

#define _CLI_SIZEOF_POINTEE(ptr) sizeof(*(ptr))

/* 用法字符串数组辅助宏，展开为 NULL 结尾的数组指针。
 * 数组元素个数由 _EXPORT_CLI_COMMAND_SYMBOL 内部通过 sizeof 自动计算。
 * 外层双层圆括号是为了避免预处理器把大括号内的逗号当成宏参数分隔符。 */
#define USAGE(...) \
	((char *[]){ __VA_ARGS__, NULL })

#define CLI_COMMAND(name, cmd_str, brief_str, _usage_arr, parse_cb,        \
		    arg_struct_ptr, ...)                                     \
	/* 定义选项数组（放在全局区） */                                     \
	cli_option_t _cli_options_##name[] = { __VA_ARGS__ };          \
                                                                             \
	/* 通过链接脚本段收集注册，arg_buf 在运行时分派时从内存池申请 */     \
	/* entry 和 exit 留空，scheduler 通过 entry/exit 是否为 NULL      \
	 * 判断这是旧式命令（执行一次即退出） */                           \
	_EXPORT_CLI_COMMAND_SYMBOL(                                          \
		name, cmd_str, brief_str, _usage_arr,                        \
		_CLI_SIZEOF_POINTEE(arg_struct_ptr),                         \
		_cli_options_##name,                                         \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),        \
		NULL,                                                        \
		(int (*)(void *))parse_cb,                                   \
		NULL,                                                        \
		NULL, CLI_CMD_BUF_SIZE, ".cli_commands")

#define CLI_COMMAND_WITH_BUF(name, cmd_str, brief_str, _usage_arr,           \
				 parse_cb, arg_struct_ptr, buf,            \
				 buf_size, ...)                            \
	/* 定义选项数组（放在全局区，不加 static，同名命令注册将触发链接期多重定义错误） */ \
	cli_option_t _cli_options_##name[] = { __VA_ARGS__ };            \
                                                                               \
	/* 通过链接脚本段收集注册，使用用户指定的缓冲区 */                     \
	_EXPORT_CLI_COMMAND_SYMBOL(                                            \
		name, cmd_str, brief_str, _usage_arr,                        \
		_CLI_SIZEOF_POINTEE(arg_struct_ptr),                         \
		_cli_options_##name,                                         \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),        \
		NULL,                                                        \
		(int (*)(void *))parse_cb,                                   \
		NULL,                                                        \
		buf, buf_size, ".cli_commands")

/* 无参数结构体、无选项的命令（arg_struct_size 为 0，缓冲区从内存池申请） */
#define CLI_COMMAND_NO_STRUCT(name, cmd_str, brief_str, parse_cb)      \
	_EXPORT_CLI_COMMAND_SYMBOL(name, cmd_str, brief_str, (void *)0, 0, \
				   NULL, 0,                              \
				   NULL,                                   \
				   (int (*)(void *))parse_cb,              \
				   NULL,                                   \
				   NULL, CLI_CMD_BUF_SIZE, ".cli_commands")

#define END_OPTIONS /* 结束标记，实际为空 */

/* ============================================================
 * 新增：非阻塞三阶段命令注册宏
 * ============================================================ */

#define CLI_COMMAND_ASYNC(name, cmd_str, brief_str, _usage_arr,          \
				 _entry, _task, _exit, arg_struct_ptr,    \
				 ...)                                     \
	cli_option_t _cli_options_##name[] = { __VA_ARGS__ };              \
	_EXPORT_CLI_COMMAND_SYMBOL(                                              \
		name, cmd_str, brief_str, _usage_arr,                            \
		_CLI_SIZEOF_POINTEE(arg_struct_ptr),                             \
		_cli_options_##name,                                             \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),            \
		(void (*)(void *))_entry,                                        \
		(int (*)(void *))_task,                                          \
		(void (*)(void *))_exit,                                         \
		NULL, CLI_CMD_BUF_SIZE, ".cli_commands")

/* ============================================================
 * 新增：命令解析准备与清理接口（取代 dispose_mec 状态机）
 * ============================================================ */

int cmd_parse_prepare(char *cmd, cmd_parse_ctx_t *ctx,
		      const cli_command_t **out_cmd_def, int *cmd_ret);
void cmd_parse_cleanup(const cli_command_t *cmd_def, cmd_parse_ctx_t *ctx);



#if CLI_ENABLE_CMD_CHAIN
/* 命令链拆分工具 */
int split_cmd_chain(char *buf, char **cmds, int max_cmds);
#endif

#endif
