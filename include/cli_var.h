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
 * 数据结构定义
 * ============================================================ */

typedef struct cli_var {
	const char *name;
	cli_type_t type;
	void *addr;
	size_t size;
	const char *doc;
	bool readonly;
} cli_var_t;

/* ============================================================
 * 链接脚本段收集符号声明
 * ============================================================ */

extern const cli_var_t *const _cli_vars_start[];
extern const cli_var_t *const _cli_vars_end[];

#define _FOR_EACH_CLI_VAR(_start, _end, _var)                          \
	for (const cli_var_t *const *_pp = (_start);                       \
	     _pp < (const cli_var_t *const *)(_end); _pp++)                \
		if (((_var) = *_pp) != NULL)

/* ============================================================
 * 变量注册宏
 * ============================================================
 *
 * CLI_VAR(symbol, name, TYPE, doc)      -- 可读写变量
 * CLI_VAR_RO(symbol, name, TYPE, doc)   -- 只读变量
 *
 * 关键设计：宏直接使用 symbol 本身，编译器自动推导：
 *   - &(_symbol)   → 变量地址
 *   - sizeof(_symbol) → 变量/数组大小（编译期常量）
 *
 * 对于 char buf[32]，sizeof(buf) = 32，框架据此做边界检查。
 * 对于 char *p，sizeof(p) = 指针大小（通常为 8），需避免。
 */

#define _CLI_VAR_REGISTER(_symbol, _name, _type, _doc, _ro)            \
	static const cli_var_t _cli_var_def_##_symbol = {                  \
		.name = _name,                                             \
		.type = _type,                                             \
		.addr = (void *)&(_symbol),                                \
		.size = sizeof(_symbol),                                   \
		.doc = _doc,                                               \
		.readonly = _ro,                                           \
	};                                                                 \
	static const cli_var_t *const _cli_var_ptr_##_symbol               \
		__attribute__((used, section(".cli_vars.1"))) =            \
			&_cli_var_def_##_symbol

#define CLI_VAR(_symbol, _name, _type, _doc)                           \
	_CLI_VAR_REGISTER(_symbol, _name, CLI_TYPE_##_type, _doc, false)

#define CLI_VAR_RO(_symbol, _name, _type, _doc)                        \
	_CLI_VAR_REGISTER(_symbol, _name, CLI_TYPE_##_type, _doc, true)

/* ============================================================
 * 函数接口
 * ============================================================ */

const cli_var_t *cli_var_find(const char *name);
void cli_var_print(const cli_var_t *var);
int cli_var_set(const cli_var_t *var, const char *value);
void cli_var_list_all(void);

/* 变量命令统一分派入口（cmd_dispose.c 调用） */
int cli_var_dispatch(int argc, char **argv);

#endif
