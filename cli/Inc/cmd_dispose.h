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
	CLI_TYPE_DOUBLE, // 浮点数
	CLI_TYPE_CALLBACK, // 自定义回调处理
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
	const char *doc; // 命令说明
	void *arg_struct; // 参数结构体指针（运行时填充）
	size_t arg_struct_size; // 结构体大小
	cli_option_t *options; // 选项数组
	size_t option_count; // 选项数量
	int (*validator)(void *); // 自定义验证函数（旧接口兼容）
	void (*cmd_entry)(void *); // 命令入口，只执行一次
	int (*cmd_task)(void *); // 命令主体，每次调度器轮询执行一次
	void (*cmd_exit)(void *); // 命令出口，只执行一次
	void *arg_buf; // 命令参数解析缓冲区指针
	size_t arg_buf_size; // 缓冲区大小
} cli_command_t;

struct alias_cmd {
	char *alias_name; // 重命名后的名字，例如 "ll"
	char *original_name; // 原始命令，例如 "ls -l"
};

/* ============================================================
 * 链接脚本段收集符号声明
 * ============================================================ */

extern const cli_command_t *const _cli_commands_start[];
extern const cli_command_t *const _cli_commands_end[];

extern struct alias_cmd *const _alias_cmd_start[];
extern struct alias_cmd *const _alias_cmd_end[];

#define _FOR_EACH_CLI_COMMAND(_start, _end, _cmd)               \
	for (const cli_command_t *const *_pp = (_start);        \
	     _pp < (const cli_command_t *const *)(_end); _pp++) \
		if (((_cmd) = *_pp) != NULL)

/* ============================================================
 * 宏工具：计算偏移量
 * ============================================================ */

#define CLI_OFFSETOF(type, field) offsetof(type, field)

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

#define _EXPORT_CLI_COMMAND_SYMBOL(_obj, _cmd_str, _doc_str, _size, _opts,     \
				   _opts_cnt, _vld, _entry, _task, _exit,    \
				   _buf, _buf_size, _section)                \
	static const cli_command_t _cli_cmd_def_##_obj = {                     \
		.name = _cmd_str,                                              \
		.doc = _doc_str,                                               \
		.arg_struct = NULL,                                            \
		.arg_struct_size = _size,                                      \
		.options = _opts,                                              \
		.option_count = _opts_cnt,                                     \
		.validator = _vld,                                             \
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

#define _CLI_SIZEOF_POINTEE(ptr) \
	((size_t)(((char *)((ptr) + 1)) - ((char *)(ptr))))

#define CLI_COMMAND(name, cmd_str, doc_str, parse_cb, arg_struct_ptr, ...)   \
	/* 定义选项数组（放在全局区） */                                     \
	cli_option_t _cli_options_##name[] = { __VA_ARGS__ };          \
                                                                             \
	/* 通过链接脚本段收集注册，arg_buf 在运行时分派时从内存池申请 */     \
	/* 旧接口兼容：parse_cb 同时填入 validator 和 cmd_task，          \
	 * entry 和 exit 留空，scheduler 通过 entry/exit 是否为 NULL      \
	 * 判断这是旧式命令（执行一次即退出） */                           \
	_EXPORT_CLI_COMMAND_SYMBOL(                                          \
		name, cmd_str, doc_str, _CLI_SIZEOF_POINTEE(arg_struct_ptr), \
		_cli_options_##name,                                         \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),        \
		(int (*)(void *))parse_cb,                                   \
		NULL,                                                        \
		(int (*)(void *))parse_cb,                                   \
		NULL,                                                        \
		NULL, CLI_CMD_BUF_SIZE, ".cli_commands")

#define CLI_COMMAND_WITH_BUF(name, cmd_str, doc_str, parse_cb, arg_struct_ptr, \
			     buf, buf_size, ...)                               \
	/* 定义选项数组（放在静态区） */                                       \
	static cli_option_t _cli_options_##name[] = { __VA_ARGS__ };     \
                                                                               \
	/* 通过链接脚本段收集注册，使用用户指定的缓冲区 */                     \
	_EXPORT_CLI_COMMAND_SYMBOL(                                            \
		name, cmd_str, doc_str, _CLI_SIZEOF_POINTEE(arg_struct_ptr),   \
		_cli_options_##name,                                           \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),          \
		(int (*)(void *))parse_cb,                                     \
		NULL,                                                          \
		(int (*)(void *))parse_cb,                                     \
		NULL,                                                          \
		buf, buf_size, ".cli_commands")

/* 无参数结构体、无选项的命令（arg_struct_size 为 0，缓冲区从内存池申请） */
#define CLI_COMMAND_NO_STRUCT(name, cmd_str, doc_str, parse_cb)        \
	_EXPORT_CLI_COMMAND_SYMBOL(name, cmd_str, doc_str, 0, NULL, 0, \
				   (int (*)(void *))parse_cb, NULL,    \
				   (int (*)(void *))parse_cb,          \
				   NULL,                               \
				   NULL, CLI_CMD_BUF_SIZE, ".cli_commands")

#define END_OPTIONS /* 结束标记，实际为空 */

#define CMD_ALIAS(new, origin)                                             \
	struct alias_cmd alias_cmd##new = {                                \
		.alias_name = #new,                                        \
		.original_name = origin,                                   \
	};                                                                 \
	static struct alias_cmd *const alias_cmd_ptr##new                  \
		__attribute__((used, section(".alias_cmd.1"))) =           \
			&alias_cmd##new;                                   \
	_EXPORT_CLI_COMMAND_SYMBOL(cmd_alias##new, #new, NULL, 0, NULL, 0, \
				   (int (*)(void *))NULL, NULL,          \
				   NULL, NULL, NULL, 0,                  \
				   ".cli_commands")

#define FOR_EACH_ALIAS(_start, _end, alias_cmd)              \
	for (struct alias_cmd *const *_pp = (_start);        \
	     _pp < (struct alias_cmd *const *)(_end); _pp++) \
		if (((alias_cmd) = *_pp) != NULL)

/* ============================================================
 * 新增：非阻塞三阶段命令注册宏
 * ============================================================ */

#define CLI_COMMAND_ASYNC(name, cmd_str, doc_str, _entry, _task, _exit,        \
			    arg_struct_ptr, ...)                               \
	cli_option_t _cli_options_##name[] = { __VA_ARGS__ };              \
	_EXPORT_CLI_COMMAND_SYMBOL(                                              \
		name, cmd_str, doc_str, _CLI_SIZEOF_POINTEE(arg_struct_ptr),     \
		_cli_options_##name,                                             \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),            \
		NULL,                                                            \
		(void (*)(void *))_entry,                                        \
		(int (*)(void *))_task,                                          \
		(void (*)(void *))_exit,                                         \
		NULL, CLI_CMD_BUF_SIZE, ".cli_commands")

/* ============================================================
 * 新增：命令解析准备与清理接口（取代 dispose_mec 状态机）
 * ============================================================ */

int cmd_parse_prepare(char *cmd, const cli_command_t **out_cmd_def,
		      int *cmd_ret);
void cmd_parse_cleanup(const cli_command_t *cmd_def);



/* 命令链拆分工具 */
int split_cmd_chain(char *buf, char **cmds, int max_cmds);

/* 别名替换工具 */
char *alias_replace(char *cmd, char *buf, size_t buf_size);

#endif
