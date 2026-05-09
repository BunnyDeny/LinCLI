/*
 * LinCLI - Variable export system for embedded CLI.
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

#ifndef _CLI_VAR_H_
#define _CLI_VAR_H_

#include <stdbool.h>
#include <stddef.h>
#include "cmd_dispose.h"

/* ============================================================
 *  自定义类型操作接口
 * ============================================================
 *
 * 用户可以通过 CLI_VAR_TYPE 宏注册自定义类型，
 * 并指定该类型在 var 命令中的序列化/反序列化规则。
 *
 * 框架内建类型（INT / DOUBLE / BOOL / STRING）同样基于
 * 这套机制实现，对用户完全透明。
 */

typedef struct cli_var_type_ops {
	int (*from_string)(void *addr, size_t size, const char *str);
	int (*to_string)(const void *addr, size_t size, char *buf,
			 size_t buf_size);
} cli_var_type_ops_t;

typedef struct cli_var_type {
	const char *name;
	cli_var_type_ops_t ops;
} cli_var_type_t;

/* ============================================================
 *  数据结构定义
 * ============================================================ */

typedef struct cli_var {
	const char *name;
	cli_type_t type;
	const char *type_name; /* 类型名，如 "INT" / "point" */
	void *addr;
	size_t size;
	const char *doc;
	bool readonly;
} cli_var_t;

/* ============================================================
 *  链接脚本段收集符号声明
 * ============================================================ */

extern const cli_var_t *const _cli_vars_start[];
extern const cli_var_t *const _cli_vars_end[];

#define _FOR_EACH_CLI_VAR(_start, _end, _var)               \
	for (const cli_var_t *const *_pp = (_start);        \
	     _pp < (const cli_var_t *const *)(_end); _pp++) \
		if (((_var) = *_pp) != NULL)

/* 自定义类型段 */
extern const cli_var_type_t *const _cli_var_types_start[];
extern const cli_var_type_t *const _cli_var_types_end[];

#define _FOR_EACH_CLI_VAR_TYPE(_start, _end, _type)                 \
	for (const cli_var_type_t *const *_pp = (_start);               \
	     _pp < (const cli_var_type_t *const *)(_end); _pp++)        \
		if (((_type) = *_pp) != NULL)

/* ============================================================
 * 变量注册宏
 * ============================================================
 *
 * CLI_VAR(symbol, name, TYPE, doc)      -- 可读写变量
 * CLI_VAR_RO(symbol, name, TYPE, doc)   -- 只读变量
 * CLI_VAR_CUSTOM(symbol, name, type_name, doc)    -- 可读写变量（显式自定义类型）
 * CLI_VAR_CUSTOM_RO(symbol, name, type_name, doc) -- 只读变量（显式自定义类型）
 *
 * 关键设计：宏直接使用 symbol 本身，编译器自动推导：
 *   - &(_symbol)   → 变量地址
 *   - sizeof(_symbol) → 变量/数组大小（编译期常量）
 *
 * 对于 char buf[32]，sizeof(buf) = 32，框架据此做边界检查。
 * 对于 char *p，sizeof(p) = 指针大小（通常为 8），需避免。
 */

#if CLI_ENABLE_VAR
#define _CLI_VAR_REGISTER(_symbol, _name, _type_name, _doc, _ro) \
	static const cli_var_t _cli_var_def_##_symbol = {          \
		.name = _name,                                       \
		.type = CLI_TYPE_CUSTOM,                             \
		.type_name = _type_name,                             \
		.addr = (void *)&(_symbol),                          \
		.size = sizeof(_symbol),                             \
		.doc = _doc,                                         \
		.readonly = _ro,                                     \
	};                                                       \
	static const cli_var_t *const _cli_var_ptr_##_symbol     \
		__attribute__((used, section(".cli_vars.1"))) =  \
			&_cli_var_def_##_symbol

/* 内建类型变量：TYPE 会被预处理器字符串化为 "INT" / "DOUBLE" / "BOOL" / "STRING" */
#define CLI_VAR(_symbol, _name, _type, _doc) \
	_CLI_VAR_REGISTER(_symbol, _name, #_type, _doc, false)

#define CLI_VAR_RO(_symbol, _name, _type, _doc) \
	_CLI_VAR_REGISTER(_symbol, _name, #_type, _doc, true)

/* 显式自定义类型变量注册宏 */
#define CLI_VAR_CUSTOM(_symbol, _name, _type_name, _doc) \
	_CLI_VAR_REGISTER(_symbol, _name, _type_name, _doc, false)

#define CLI_VAR_CUSTOM_RO(_symbol, _name, _type_name, _doc) \
	_CLI_VAR_REGISTER(_symbol, _name, _type_name, _doc, true)

/* 类型注册宏（全局只需注册一次，可被多个变量共享） */
#define CLI_VAR_TYPE(_name, _from_str, _to_str)                     \
	static const cli_var_type_t _cli_vartype_def_##_name = {        \
		.name = #_name,                                             \
		.ops = { .from_string = _from_str, .to_string = _to_str },  \
	};                                                              \
	static const cli_var_type_t *const _cli_vartype_ptr_##_name     \
		__attribute__((used, section(".cli_var_types.1"))) =        \
			&_cli_vartype_def_##_name
#else
#define _CLI_VAR_REGISTER(_symbol, _name, _type_name, _doc, _ro) /* disabled */
#define CLI_VAR(_symbol, _name, _type, _doc) /* disabled */
#define CLI_VAR_RO(_symbol, _name, _type, _doc) /* disabled */
#define CLI_VAR_CUSTOM(_symbol, _name, _type_name, _doc) /* disabled */
#define CLI_VAR_CUSTOM_RO(_symbol, _name, _type_name, _doc) /* disabled */
#define CLI_VAR_TYPE(_name, _from_str, _to_str) /* disabled */
#endif

#endif
