/*
 * LinCLI - Variable export system for embedded CLI.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
	const char *type_name; /* 自定义类型名，内建类型为 NULL */
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
 * CLI_VAR(symbol, name, TYPE, doc)      -- 可读写变量（内建类型）
 * CLI_VAR_RO(symbol, name, TYPE, doc)   -- 只读变量（内建类型）
 * CLI_VAR_CUSTOM(symbol, name, type_name, doc)    -- 可读写变量（自定义类型）
 * CLI_VAR_CUSTOM_RO(symbol, name, type_name, doc) -- 只读变量（自定义类型）
 *
 * 关键设计：宏直接使用 symbol 本身，编译器自动推导：
 *   - &(_symbol)   → 变量地址
 *   - sizeof(_symbol) → 变量/数组大小（编译期常量）
 *
 * 对于 char buf[32]，sizeof(buf) = 32，框架据此做边界检查。
 * 对于 char *p，sizeof(p) = 指针大小（通常为 8），需避免。
 */

#define _CLI_VAR_REGISTER(_symbol, _name, _type, _doc, _ro)     \
	static const cli_var_t _cli_var_def_##_symbol = {       \
		.name = _name,                                  \
		.type = _type,                                  \
		.type_name = NULL,                              \
		.addr = (void *)&(_symbol),                     \
		.size = sizeof(_symbol),                        \
		.doc = _doc,                                    \
		.readonly = _ro,                                \
	};                                                      \
	static const cli_var_t *const _cli_var_ptr_##_symbol    \
		__attribute__((used, section(".cli_vars.1"))) = \
			&_cli_var_def_##_symbol

#define CLI_VAR(_symbol, _name, _type, _doc) \
	_CLI_VAR_REGISTER(_symbol, _name, CLI_TYPE_##_type, _doc, false)

#define CLI_VAR_RO(_symbol, _name, _type, _doc) \
	_CLI_VAR_REGISTER(_symbol, _name, CLI_TYPE_##_type, _doc, true)

/* 自定义类型变量注册宏 */
#define _CLI_VAR_REGISTER_CUSTOM(_symbol, _name, _type_name, _doc, _ro) \
	static const cli_var_t _cli_var_def_##_symbol = {                   \
		.name = _name,                                                  \
		.type = CLI_TYPE_CUSTOM,                                        \
		.type_name = _type_name,                                        \
		.addr = (void *)&(_symbol),                                     \
		.size = sizeof(_symbol),                                        \
		.doc = _doc,                                                    \
		.readonly = _ro,                                                \
	};                                                                  \
	static const cli_var_t *const _cli_var_ptr_##_symbol                \
		__attribute__((used, section(".cli_vars.1"))) =                 \
			&_cli_var_def_##_symbol

#define CLI_VAR_CUSTOM(_symbol, _name, _type_name, _doc) \
	_CLI_VAR_REGISTER_CUSTOM(_symbol, _name, _type_name, _doc, false)

#define CLI_VAR_CUSTOM_RO(_symbol, _name, _type_name, _doc) \
	_CLI_VAR_REGISTER_CUSTOM(_symbol, _name, _type_name, _doc, true)

/* 自定义类型注册宏（全局只需注册一次，可被多个变量共享） */
#define CLI_VAR_TYPE(_name, _from_str, _to_str)                     \
	static const cli_var_type_t _cli_vartype_def_##_name = {        \
		.name = #_name,                                             \
		.ops = { .from_string = _from_str, .to_string = _to_str },  \
	};                                                              \
	static const cli_var_type_t *const _cli_vartype_ptr_##_name     \
		__attribute__((used, section(".cli_var_types.1"))) =        \
			&_cli_vartype_def_##_name

#endif
