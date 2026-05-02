/*
 * LinCLI - Environment variable system for command-line substitution.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef _CLI_ENV_H_
#define _CLI_ENV_H_

#include <stddef.h>

/* ============================================================
 *  数据结构定义
 * ============================================================
 *
 * 环境变量存储在可写段中，支持运行时通过 env --set 修改值。
 * dyn_value 指向内存池分配的动态值；为 NULL 时回退到初始 value。
 */

typedef struct cli_env {
	int id;
	const char *name;
	const char *value;
	char *dyn_value;
} cli_env_t;

/* ============================================================
 *  链接脚本段收集符号声明
 * ============================================================ */

extern cli_env_t *const _cli_envs_start[];
extern cli_env_t *const _cli_envs_end[];

#define _FOR_EACH_CLI_ENV(_start, _end, _env)                     \
	for (cli_env_t *const *_pp = (_start);                        \
	     _pp < (cli_env_t *const *)(_end); _pp++)                 \
		if (((_env) = *_pp) != NULL)

/* ============================================================
 *  环境变量注册宏
 * ============================================================
 *
 * 用法：
 *   CLI_ENV(NAME, "value")
 *   CLI_ENV(PATH, "/bin:/usr/bin")
 *
 * 注册后可在命令行中通过 $NAME 或 $id 引用。
 * 注意：名字为纯整数的注册会被运行时忽略。
 */

#define CLI_ENV(_name, _value)                                        \
	static cli_env_t _cli_env_def_##_name = {                         \
		.id = -1,                                                     \
		.name = #_name,                                               \
		.value = _value,                                              \
		.dyn_value = NULL,                                            \
	};                                                                \
	static cli_env_t *const _cli_env_ptr_##_name                      \
		__attribute__((used, section(".cli_envs.1"))) =               \
			&_cli_env_def_##_name

/* ============================================================
 *  公共接口
 * ============================================================ */

cli_env_t *cli_env_find(const char *name);
cli_env_t *cli_env_find_by_id(int id);
const char *cli_env_get_value(const cli_env_t *env);
int cli_env_set(cli_env_t *env, const char *new_value);
int cli_env_replace(const char *input, char *out, size_t out_size);

#endif
