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
#include <getopt.h>
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
	const char *depends; // 依赖的选项（字符串形式）。
	// 若以 '!' 开头，则表示与后续选项名互斥。
} cli_option_t;

typedef struct cli_command {
	const char *name; // 命令名
	const char *doc; // 命令说明
	void *arg_struct; // 参数结构体指针（运行时填充）
	size_t arg_struct_size; // 结构体大小
	const cli_option_t *options; // 选项数组
	size_t option_count; // 选项数量
	int (*validator)(void *); // 自定义验证函数
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
		if (((_cmd) = *_pp))

/* ============================================================
 * 宏工具：计算偏移量
 * ============================================================ */

#define CLI_OFFSETOF(type, field) offsetof(type, field)

/* ============================================================
 * OPTION 宏定义（统一入口）
 * ============================================================
 *
 * 统一注册一个命令选项。该宏根据传入的参数数量自动分派到对应的
 * OPTION_x 实现（x = 6~9），用户无需手动选择底层宏名。
 *
 * 使用方式：数一下你需要的参数个数，直接按对应格式填写即可。
 */

#define _GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME

#define OPTION(...)                                           \
	_GET_MACRO(__VA_ARGS__, OPTION_9, OPTION_8, OPTION_7, \
		   OPTION_6)(__VA_ARGS__)

/**
 * @brief 基础类型选项，无额外属性（6 个参数）。
 *
 * 适用类型：BOOL / STRING / INT / DOUBLE / CALLBACK
 *
 * @param _sopt    短选项字符，例如 'v'
 * @param _lopt    长选项字符串，例如 "verbose"
 * @param _type    选项类型标识，例如 BOOL、STRING、INT
 * @param _help    帮助说明文本
 * @param _stype   用户定义参数结构体类型，例如 struct my_args
 * @param _field   该选项对应结构体中的字段名
 *
 * 使用示例：
 *   OPTION('v', "verbose", BOOL, "Enable verbose", struct my_args, verbose)
 */
#define OPTION_6(_sopt, _lopt, _type, _help, _stype, _field)           \
	{                                                              \
		.short_opt = _sopt,                                    \
		.long_opt = _lopt,                                     \
		.type = CLI_TYPE_##_type,                              \
		.help = _help,                                         \
		.offset = CLI_OFFSETOF(_stype, _field),                \
		.offset_count = _OPTION_COUNT_##_type(_stype, _field), \
		.max_args = 1,                                         \
		.required = false,                                     \
		.depends = NULL,                                       \
	}

/**
 * @brief 基础类型选项，带 required 开关（7 个参数）。
 *
 * 适用类型：BOOL / STRING / INT / DOUBLE / CALLBACK
 *
 * @param _sopt    短选项字符
 * @param _lopt    长选项字符串
 * @param _type    选项类型标识
 * @param _help    帮助说明文本
 * @param _stype   用户定义参数结构体类型
 * @param _field   该选项对应结构体中的字段名
 * @param _req     是否为必需选项，填 true 或 false
 *
 * 使用示例：
 *   OPTION('v', "verbose", BOOL, "Enable verbose", struct my_args, verbose, true)
 */
#define OPTION_7(_sopt, _lopt, _type, _help, _stype, _field, _req)     \
	{                                                              \
		.short_opt = _sopt,                                    \
		.long_opt = _lopt,                                     \
		.type = CLI_TYPE_##_type,                              \
		.help = _help,                                         \
		.offset = CLI_OFFSETOF(_stype, _field),                \
		.offset_count = _OPTION_COUNT_##_type(_stype, _field), \
		.max_args = 1,                                         \
		.required = _req,                                      \
		.depends = NULL,                                       \
	}

/**
 * @brief 数组类型选项，带最大参数个数和依赖项（8 个参数）。
 *
 * 适用类型：INT_ARRAY
 *
 * @param _sopt    短选项字符
 * @param _lopt    长选项字符串
 * @param _type    选项类型标识，此处固定为 INT_ARRAY
 * @param _help    帮助说明文本
 * @param _stype   用户定义参数结构体类型
 * @param _field   该选项对应结构体中的字段名（数组指针）
 * @param _max     该数组选项允许接收的最大参数个数
 * @param _dep     依赖的另一个选项名字符串（例如 "verbose"），
 *                 若无需依赖项，请显式填 NULL。
 *
 * 使用示例：
 *   OPTION('n', "nums", INT_ARRAY, "Number list", struct my_args, nums, 8, NULL)
 *   OPTION('n', "nums", INT_ARRAY, "Number list", struct my_args, nums, 8, "verbose")
 */
#define OPTION_8(_sopt, _lopt, _type, _help, _stype, _field, _max, _dep) \
	{                                                                \
		.short_opt = _sopt,                                      \
		.long_opt = _lopt,                                       \
		.type = CLI_TYPE_##_type,                                \
		.help = _help,                                           \
		.offset = CLI_OFFSETOF(_stype, _field),                  \
		.offset_count = _OPTION_COUNT_##_type(_stype, _field),   \
		.max_args = _max,                                        \
		.required = false,                                       \
		.depends = _dep,                                         \
	}

/**
 * @brief 数组类型选项，带最大参数个数、依赖项和 required 开关（9 个参数）。
 *
 * 适用类型：INT_ARRAY
 *
 * @param _sopt    短选项字符
 * @param _lopt    长选项字符串
 * @param _type    选项类型标识，此处固定为 INT_ARRAY
 * @param _help    帮助说明文本
 * @param _stype   用户定义参数结构体类型
 * @param _field   该选项对应结构体中的字段名（数组指针）
 * @param _max     该数组选项允许接收的最大参数个数
 * @param _dep     依赖的另一个选项名字符串，若无需依赖项请填 NULL
 * @param _req     是否为必需选项，填 true 或 false
 *
 * 使用示例：
 *   OPTION('n', "nums", INT_ARRAY, "Number list", struct my_args, nums, 8, "verbose", true)
 */
#define OPTION_9(_sopt, _lopt, _type, _help, _stype, _field, _max, _dep, _req) \
	{                                                                      \
		.short_opt = _sopt,                                            \
		.long_opt = _lopt,                                             \
		.type = CLI_TYPE_##_type,                                      \
		.help = _help,                                                 \
		.offset = CLI_OFFSETOF(_stype, _field),                        \
		.offset_count = _OPTION_COUNT_##_type(_stype, _field),         \
		.max_args = _max,                                              \
		.required = _req,                                              \
		.depends = _dep,                                               \
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
				   _opts_cnt, _vld, _buf, _buf_size, _section) \
	static const cli_command_t _cli_cmd_def_##_obj = {                     \
		.name = _cmd_str,                                              \
		.doc = _doc_str,                                               \
		.arg_struct = NULL,                                            \
		.arg_struct_size = _size,                                      \
		.options = _opts,                                              \
		.option_count = _opts_cnt,                                     \
		.validator = _vld,                                             \
		.arg_buf = _buf,                                               \
		.arg_buf_size = _buf_size,                                     \
	};                                                                     \
	static const cli_command_t *const _cli_cmd_ptr_##_obj                  \
		__attribute__((used, section(_section))) =                     \
			&_cli_cmd_def_##_obj

/* 全局共享命令参数缓冲区，所有使用 CLI_COMMAND 注册的命令串行复用该内存。
 * 由于 CLI 解析与执行在单一线程中串行完成，不会产生并发冲突。
 * 若用户结构体超过此大小，请使用 CLI_COMMAND_WITH_BUF 宏自行指定缓冲区。
 */
#ifndef CLI_CMD_BUF_SIZE
#define CLI_CMD_BUF_SIZE 128
#endif
extern char g_cli_cmd_buf[CLI_CMD_BUF_SIZE];

#define CLI_COMMAND(name, cmd_str, doc_str, parse_cb, arg_struct_ptr, ...)  \
	/* 前向声明参数结构体类型 */                                        \
	typedef typeof(*arg_struct_ptr) _cli_struct_##name;                 \
                                                                            \
	/* 定义选项数组（放在全局区） */                                    \
	const cli_option_t _cli_options_##name[] = { __VA_ARGS__ };         \
                                                                            \
	/* 通过链接脚本段收集注册，使用全局共享缓冲区 */                    \
	_EXPORT_CLI_COMMAND_SYMBOL(                                         \
		name, cmd_str, doc_str, sizeof(_cli_struct_##name),         \
		_cli_options_##name,                                        \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),       \
		(int (*)(void *))parse_cb, g_cli_cmd_buf, CLI_CMD_BUF_SIZE, \
		".cli_commands")

#define CLI_COMMAND_WITH_BUF(name, cmd_str, doc_str, parse_cb, arg_struct_ptr, \
			     buf, buf_size, ...)                               \
	/* 前向声明参数结构体类型 */                                           \
	typedef typeof(*arg_struct_ptr) _cli_struct_##name;                    \
                                                                               \
	/* 定义选项数组（放在静态区） */                                       \
	static const cli_option_t _cli_options_##name[] = { __VA_ARGS__ };     \
                                                                               \
	/* 通过链接脚本段收集注册，使用用户指定的缓冲区 */                     \
	_EXPORT_CLI_COMMAND_SYMBOL(                                            \
		name, cmd_str, doc_str, sizeof(_cli_struct_##name),            \
		_cli_options_##name,                                           \
		(sizeof(_cli_options_##name) / sizeof(cli_option_t)),          \
		(int (*)(void *))parse_cb, buf, buf_size, ".cli_commands")

#define END_OPTIONS /* 结束标记，实际为空 */

#define CMD_ALIAS(new, origin, doc_str)                        \
	struct alias_cmd alias_cmd##new = {                    \
		.alias_name = #new,                            \
		.original_name = origin,                       \
	};                                                     \
	static struct alias_cmd *const alias_cmd_ptr##new      \
		__attribute__((used, section(".alias_cmd"))) = \
			&alias_cmd##new;                       \
	CLI_COMMAND(cmd_alias##new, #new, doc_str, NULL, NULL, END_OPTIONS);

#define FOR_EACH_ALIAS(_start, _end, alias_cmd)              \
	for (struct alias_cmd *const *_pp = (_start);        \
	     _pp < (struct alias_cmd *const *)(_end); _pp++) \
		if (((alias_cmd) = *_pp))

int dispose_init(void);
int dispose_task(char *cmd, int *cmd_ret);

#endif
