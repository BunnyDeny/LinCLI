/*
 * LinCLI - Environment variable system implementation.
 * Copyright (C) 2026  bunnydeny <guoy55448@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "cli_env.h"
#include "cli_errno.h"
#include "cli_io.h"
#include "cmd_dispose.h"
#include <string.h>

/* ============================================================
 *  查找环境变量
 * ============================================================ */

const cli_env_t *cli_env_find(const char *name)
{
	const cli_env_t *env;
	_FOR_EACH_CLI_ENV(_cli_envs_start, _cli_envs_end, env)
	{
		if (env->name && strcmp(env->name, name) == 0)
			return env;
	}
	return NULL;
}

/* ============================================================
 *  字符串替换辅助函数
 * ============================================================ */

static bool is_env_name_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_';
}

static int extract_env_name(const char *src, int pos, char *name_buf,
			    size_t name_buf_size)
{
	int i = pos;
	size_t j = 0;
	while (src[i] && is_env_name_char(src[i]) && j < name_buf_size - 1) {
		name_buf[j] = src[i];
		j++;
		i++;
	}
	name_buf[j] = '\0';
	return i - pos;
}

static int append_to_buf(char *dst, size_t dst_size, int dst_pos,
			 const char *src, int len)
{
	int i = 0;
	while (i < len && dst_pos < (int)dst_size - 1) {
		dst[dst_pos] = src[i];
		dst_pos++;
		i++;
	}
	return dst_pos;
}

static int append_value(char *dst, size_t dst_size, int dst_pos,
			const char *value)
{
	return append_to_buf(dst, dst_size, dst_pos, value,
			     (int)strlen(value));
}

static int substitute_one(const char *input, int *in_pos, char *out,
			  size_t out_size, int *out_pos,
			  char *name_buf, size_t name_buf_size)
{
	(*in_pos)++;
	int name_len = extract_env_name(input, *in_pos, name_buf,
					name_buf_size);
	if (name_len == 0) {
		*out_pos = append_to_buf(out, out_size, *out_pos, "$", 1);
		return CLI_OK;
	}
	*in_pos += name_len;

	const cli_env_t *env = cli_env_find(name_buf);
	if (env && env->value) {
		*out_pos = append_value(out, out_size, *out_pos, env->value);
	} else {
		*out_pos = append_to_buf(out, out_size, *out_pos, "$", 1);
		*out_pos = append_to_buf(out, out_size, *out_pos, name_buf,
					 name_len);
	}
	return CLI_OK;
}

static int do_substitution(const char *input, char *out, size_t out_size)
{
	int in_pos = 0;
	int out_pos = 0;
	char name_buf[32];

	while (input[in_pos] && out_pos < (int)out_size - 1) {
		if (input[in_pos] == '$' && is_env_name_char(input[in_pos + 1])) {
			substitute_one(input, &in_pos, out, out_size, &out_pos,
				     name_buf, sizeof(name_buf));
		} else {
			out[out_pos] = input[in_pos];
			out_pos++;
			in_pos++;
		}
	}
	out[out_pos] = '\0';
	return input[in_pos] == '\0' ? CLI_OK : CLI_ERR_BUF_INSUFF;
}

/* ============================================================
 *  公共接口：环境变量替换
 * ============================================================
 *
 * 将 input 中所有 $NAME 形式的引用替换为对应环境变量的值。
 * 未定义的变量保留原样（包括 $ 符号）。
 * 只做一轮非递归替换。
 */

int cli_env_replace(const char *input, char *out, size_t out_size)
{
	if (!input || !out || out_size == 0)
		return CLI_ERR_NULL;
	out[0] = '\0';
	return do_substitution(input, out, out_size);
}

/* ============================================================
 *  env 命令：列出所有环境变量
 * ============================================================ */

static void cli_env_list_all(void)
{
	const cli_env_t *env;
	all_printk("\r\n%-20s %s\r\n", "NAME", "VALUE");
	all_printk("--------------------------------------------\r\n");
	_FOR_EACH_CLI_ENV(_cli_envs_start, _cli_envs_end, env)
	{
		all_printk("%-20s %s\r\n", env->name,
			   env->value ? env->value : "");
	}
}

static int env_handler(void *_args)
{
	(void)_args;
	cli_env_list_all();
	return 0;
}

CLI_COMMAND_NO_STRUCT(env, "env", "list all environment variables",
		      env_handler);
