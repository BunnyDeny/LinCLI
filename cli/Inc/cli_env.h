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
 * ============================================================ */

typedef struct cli_env {
	const char *name;
	const char *value;
} cli_env_t;

/* ============================================================
 *  链接脚本段收集符号声明
 * ============================================================ */

extern const cli_env_t *const _cli_envs_start[];
extern const cli_env_t *const _cli_envs_end[];

#define _FOR_EACH_CLI_ENV(_start, _end, _env)                     \
	for (const cli_env_t *const *_pp = (_start);                \
	     _pp < (const cli_env_t *const *)(_end); _pp++)         \
		if (((_env) = *_pp) != NULL)

/* ============================================================
 *  环境变量注册宏
 * ============================================================
 *
 * 用法：
 *   CLI_ENV(NAME, "value")
 *   CLI_ENV(PATH, "/bin:/usr/bin")
 *
 * 注册后可在命令行中通过 $NAME 引用：
 *   echo $PATH
 */

#define CLI_ENV(_name, _value)                                        \
	static const cli_env_t _cli_env_def_##_name = {                   \
		.name = #_name,                                               \
		.value = _value,                                              \
	};                                                                \
	static const cli_env_t *const _cli_env_ptr_##_name                \
		__attribute__((used, section(".cli_envs.1"))) =               \
			&_cli_env_def_##_name

/* ============================================================
 *  公共接口
 * ============================================================ */

const cli_env_t *cli_env_find(const char *name);
int cli_env_replace(const char *input, char *out, size_t out_size);

#endif
